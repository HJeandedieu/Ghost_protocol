#include "systems/ObjectiveSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/EventBus.h"
#include "systems/AlarmDirector.h"
#include "systems/InteractionSystem.h"
#include "world/Raycast.h"
#include "world/World.h"

namespace {
void openTiles(TileMap& map, TileType type) {
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
            if (map.tile(x, y) == type) map.setOpen(x, y, true);
}
}  // namespace

void ObjectiveSystem::applyPreset(World& world, const MissionConfig& config, int stage) {
    auto& map = world.level.map;
    if (stage >= 3) {
        world.player.collectKeycard();
        openTiles(map, TileType::ServiceDoor);
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x)
                if (map.tile(x, y) == TileType::Keycard) map.removeKeycard(x, y);
        const auto point =
            config.retryPositions[static_cast<std::size_t>(std::clamp(stage, 3, 6) - 3)];
        if (map.isPassable(point.x, point.y))
            world.player.pos = world.player.prevPos = map.tileCenter({point.x, point.y});
    }
    if (stage >= 4) {
        world.powerOn = true;
        openTiles(map, TileType::Gate);
    }
    if (stage >= 5) openTiles(map, TileType::VaultDoor);
    if (stage >= 6) {
        world.player.unlockFrontExit();
        openTiles(map, TileType::FrontDoor);
    }
}

ObjectiveSystem::ObjectiveSystem(EventBus& events, World& world, InteractionSystem& interaction,
                                 AlarmDirector& alarm, const Config& config, int stage)
    : events_(events),
      world_(world),
      interaction_(interaction),
      alarm_(alarm),
      config_(config),
      stage_(std::clamp(stage, 1, 6)) {
    auto& map = world_.level.map;
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const Vec2 position = map.tileCenter({x, y});
            if (map.tile(x, y) == TileType::VaultDoor) {
                vaultPosition_ = position;
                const std::string id = "vault:" + std::to_string(x) + ":" + std::to_string(y);
                interaction_.add(
                    {id, position, config_.mission.crack, "Hold E: crack vault | Shift+E: thermite",
                     [this](const Player&) { return world_.powerOn && !vaultOpen_; },
                     [this](World&) { openVault(); }, true, InteractionControl::Plain,
                     [this, position](float dt) {
                         crackNoiseAge_ += dt;
                         const double interval = config_.noise.crackInterval;
                         if (interval <= 0) return;
                         while (crackNoiseAge_ + 1e-7 >= interval) {
                             crackNoiseAge_ = std::max(0.0, crackNoiseAge_ - interval);
                             events_.publish(NoiseEmitted{position, config_.noise.crack,
                                                          NoiseType::Crack, "Ghost"});
                         }
                     }});
                interaction_.add({id + ":thermite", position, config_.mission.thermitePlace,
                                  "Hold Shift+E: place thermite",
                                  [this](const Player&) {
                                      return world_.powerOn && !vaultOpen_ &&
                                             thermiteRemaining_ <= 0;
                                  },
                                  [this, position](World&) {
                                      vaultPosition_ = position;
                                      thermiteRemaining_ = config_.mission.thermiteBurn;
                                      thermiteAge_ = 0;
                                      thermitePlacedThisTick_ = true;
                                      alarm_.trigger(AlarmReason::Thermite);
                                      if (thermiteRemaining_ <= 0) openVault();
                                  },
                                  false, InteractionControl::Modified});
            } else if (map.tile(x, y) == TileType::Money) {
                Bag bag;
                bag.id = "cash:" + std::to_string(x) + ":" + std::to_string(y);
                bag.pos = bag.prevPos = position;
                bag.value = config_.payout.bag;
                bags_.push_back(std::move(bag));
            }
        }
    for (std::size_t i = 0; i < bags_.size(); ++i) {
        const auto& bag = bags_[i];
        interaction_.add({bag.id, bag.pos, 0, "E: pick up cash / bag",
                          [this, i](const Player& player) {
                              return vaultOpen_ && !player.carryingBag() &&
                                     (bags_[i].state == BagState::Stack ||
                                      bags_[i].state == BagState::Dropped);
                          },
                          [this, i](World&) { collect(i); }, false, InteractionControl::Plain});
        interaction_.add(
            {bag.id + ":dye", bag.pos, config_.mission.dyeHold, "Hold Shift+E: disarm dye pack",
             [this, i](const Player&) {
                 return bags_[i].state == BagState::Stack && bags_[i].dye == DyeState::Armed;
             },
             [this, i](World&) { bags_[i].dye = DyeState::Disarmed; }, false,
             InteractionControl::Modified});
    }
    if (stage_ >= 5) openVault();
    if (stage_ >= 6) world_.player.unlockFrontExit();
    openedThisTick_ = false;
}

void ObjectiveSystem::openVault() {
    if (vaultOpen_) return;
    vaultOpen_ = true;
    openedThisTick_ = true;
    thermiteRemaining_ = 0;
    dyeAge_ = 0;
    openTiles(world_.level.map, TileType::VaultDoor);
    for (auto& bag : bags_) bag.dye = DyeState::Armed;
}

void ObjectiveSystem::spoil(Bag& bag) {
    bag.dye = DyeState::Spoiled;
    bag.value = config_.payout.spoiled;
    bag.burstAge = 0;
    events_.publish(DyePackBurst{bag.id});
}

void ObjectiveSystem::collect(std::size_t index) {
    auto& bag = bags_[index];
    if (bag.dye == DyeState::Armed) spoil(bag);
    bag.state = BagState::Carried;
    bag.pos = bag.prevPos = world_.player.pos;
    world_.player.setCarryingBag(true);
    pickedAny_ = true;
    events_.publish(BagPicked{bag.id, bag.value});
}

void ObjectiveSystem::throwBag(Vec2 direction) {
    const float length = std::hypot(direction.x, direction.y);
    if (!world_.player.carryingBag() || !std::isfinite(length) || length <= 0) return;
    for (auto& bag : bags_) {
        if (bag.state != BagState::Carried) continue;
        const Vec2 from = world_.player.pos;
        const Vec2 target{from.x + direction.x / length * config_.mission.throwDistance,
                          from.y + direction.y / length * config_.mission.throwDistance};
        const float distance = Raycast::sightDistance(from, target, world_.level.map);
        // Stay strictly on the passable side of the exact raycast boundary.
        const float travel =
            distance < config_.mission.throwDistance
                ? std::max(0.0f, distance - std::numeric_limits<float>::epsilon() *
                                                std::max(std::abs(from.x), std::abs(from.y)) * 8)
                : distance;
        bag.pos = bag.prevPos = {from.x + direction.x / length * travel,
                                 from.y + direction.y / length * travel};
        bag.state = BagState::Dropped;
        world_.player.setCarryingBag(false);
        interaction_.setPosition(bag.id, bag.pos);
        events_.publish(BagDropped{bag.id, bag.value});
        return;
    }
}

void ObjectiveSystem::completeStage() {
    events_.publish(ObjectiveCompleted{"S" + std::to_string(stage_)});
    ++stage_;
    if (stage_ == 6) {
        world_.player.unlockFrontExit();
        openTiles(world_.level.map, TileType::FrontDoor);
    }
}

void ObjectiveSystem::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0 || world_.player.dead()) return;
    const auto& map = world_.level.map;
    const auto pos = world_.player.pos;
    const auto previous = world_.player.prevPos;
    const float size = static_cast<float>(map.tileSize());
    if (stage_ == 1) {
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x) {
                if (map.tile(x, y) != TileType::ServiceDoor) continue;
                if (pos.y < y * size || pos.y >= (y + 1) * size) approachedService_ = false;
                if (map.isOpen(x, y) && previous.x < x * size && pos.x >= x * size &&
                    previous.y >= y * size && previous.y < (y + 1) * size && pos.y >= y * size &&
                    pos.y < (y + 1) * size && Raycast::hasLineOfSight(previous, pos, map))
                    approachedService_ = true;
                if (map.isOpen(x, y) && approachedService_ && pos.x >= (x + 1) * size &&
                    pos.y >= y * size && pos.y < (y + 1) * size)
                    serviceCrossed_ = true;
            }
    }
    const bool wasOpen = vaultOpen_;
    if (thermiteRemaining_ > 0 && !thermitePlacedThisTick_) {
        thermiteAge_ += dt;
        thermiteRemaining_ =
            static_cast<float>(std::max(0.0, config_.mission.thermiteBurn - thermiteAge_));
        if (thermiteRemaining_ <= 0) openVault();
    }
    if (vaultOpen_ && wasOpen && !openedThisTick_) {
        dyeAge_ += dt;
        if (dyeAge_ + 1e-6 >= config_.mission.dyeBurst)
            for (auto& bag : bags_)
                if (bag.dye == DyeState::Armed) spoil(bag);
    }
    openedThisTick_ = false;
    thermitePlacedThisTick_ = false;
    for (auto& bag : bags_) {
        bag.burstAge += dt;
        if (bag.state == BagState::Carried) bag.pos = bag.prevPos = pos;
        interaction_.setPrompt(
            bag.id, bag.state == BagState::Dropped ? "E: pick up dropped bag"
                    : bag.dye == DyeState::Armed   ? "E: take cash (armed dye) | Shift+E: disarm"
                    : bag.dye == DyeState::Spoiled ? "E: take spoiled cash"
                                                   : "E: take cash (dye disarmed)");
    }
    while ((stage_ == 1 && serviceCrossed_) || (stage_ == 2 && world_.player.hasKeycard()) ||
           (stage_ == 3 && world_.powerOn) || (stage_ == 4 && vaultOpen_) ||
           (stage_ == 5 && pickedAny_))
        completeStage();
}

const char* ObjectiveSystem::objective() const {
    switch (stage_) {
        case 1:
            return "S1  BACK DOOR: lockpick and enter the bank";
        case 2:
            return "S2  RED CARD: find the red keycard";
        case 3:
            return "S3  LIGHTS OUT: reach the breaker";
        case 4:
            return "S4  OPEN SESAME: crack vault / place thermite";
        case 5:
            return "S5  CASH AND DYE: collect a bag";
        default:
            return "S6  GET OUT: carry cash to the street";
    }
}

int ObjectiveSystem::pickedCount() const {
    return static_cast<int>(std::count_if(
        bags_.begin(), bags_.end(), [](const Bag& bag) { return bag.state != BagState::Stack; }));
}

std::vector<Entity*> ObjectiveSystem::revealables() {
    std::vector<Entity*> result;
    result.reserve(bags_.size());
    for (auto& bag : bags_)
        if (bag.state != BagState::Delivered) result.push_back(&bag);
    return result;
}
