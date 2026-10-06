#include "systems/InteractionSystem.h"

#include <algorithm>
#include <cmath>

#include "core/Config.h"
#include "core/EventBus.h"
#include "world/Raycast.h"
#include "world/World.h"

InteractionSystem::InteractionSystem(EventBus& bus) : bus_(bus) {}
void InteractionSystem::add(Interactable item) {
    items_.push_back(std::move(item));
    elapsed_.push_back(0);
}

void InteractionSystem::loadBank(World& world, const Config& config) {
    lockpickNoise_ = config.noise.lockpick;
    items_.clear();
    elapsed_.clear();
    target_ = -1;
    auto& map = world.level.map;
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const auto tile = map.tile(x, y);
            const auto id = std::to_string(x) + ":" + std::to_string(y);
            const auto position = map.tileCenter({x, y});
            if (tile == TileType::ServiceDoor) {
                add({id, position, config.mission.lockpick, "Hold E: lockpick service door",
                     [&map, x, y](const Player&) { return !map.isOpen(x, y); },
                     [x, y](World& state) { state.level.map.setOpen(x, y, true); }});
            } else if (tile == TileType::Keycard) {
                add({id, position, 0, "E: collect red keycard",
                     [](const Player& player) { return !player.hasKeycard(); },
                     [x, y](World& state) {
                         state.player.collectKeycard();
                         state.level.map.removeKeycard(x, y);
                     }});
            } else if (tile == TileType::CardDoor) {
                add({id, position, 0, "E: unlock red-card door",
                     [&map, x, y](const Player& player) {
                         return player.hasKeycard() && !map.isOpen(x, y);
                     },
                     [x, y](World& state) { state.level.map.setOpen(x, y, true); }});
            } else if (tile == TileType::Breaker) {
                add({id, position, config.mission.breakerHold, "Hold E: restore gate power",
                     [&world](const Player&) { return !world.powerOn; },
                     [](World& state) {
                         state.powerOn = true;
                         auto& tiles = state.level.map;
                         for (int ty = 0; ty < tiles.height(); ++ty)
                             for (int tx = 0; tx < tiles.width(); ++tx)
                                 if (tiles.tile(tx, ty) == TileType::Gate)
                                     tiles.setOpen(tx, ty, true);
                     }});
            } else if (tile == TileType::SecurityPanel) {
                add({id, position, config.mission.securityHold, "Hold E: loop security",
                     [&world](const Player&) {
                         return !world.securityLoopUsed && !world.alarmLoud;
                     },
                     [this, seconds = config.camera.loopSeconds](World& state) {
                         state.securityLoopUsed = true;
                         state.securityLoopRemaining = seconds;
                         bus_.publish(SecurityLooped{seconds});
                     }});
            }
        }
}

void InteractionSystem::openNormalDoors(World& world) {
    auto& map = world.level.map;
    const auto pos = world.player.pos;
    const float size = static_cast<float>(map.tileSize());
    const int tx = static_cast<int>(std::floor(pos.x / size));
    const int ty = static_cast<int>(std::floor(pos.y / size));
    for (int y = ty - 1; y <= ty + 1; ++y)
        for (int x = tx - 1; x <= tx + 1; ++x) {
            if (map.tile(x, y) != TileType::Door || map.isOpen(x, y)) continue;
            const float dx = pos.x - std::clamp(pos.x, x * size, (x + 1) * size);
            const float dy = pos.y - std::clamp(pos.y, y * size, (y + 1) * size);
            if (dx * dx + dy * dy <= world.player.radius * world.player.radius)
                map.setOpen(x, y, true);
        }
}

void InteractionSystem::setPosition(const std::string& id, Vec2 position) {
    for (auto& item : items_)
        if (item.id == id) item.position = position;
}
void InteractionSystem::setPrompt(const std::string& id, const std::string& prompt) {
    for (auto& item : items_)
        if (item.id == id) item.prompt = prompt;
}

void InteractionSystem::update(float dt, bool held, World& world, bool modified) {
    claimedThisTick_ = false;
    if (!std::isfinite(dt) || dt <= 0) return;
    openNormalDoors(world);
    const float loopBefore = world.securityLoopRemaining;
    world.securityLoopRemaining = std::max(0.0f, loopBefore - dt);
    if (loopBefore > 0 && world.securityLoopRemaining == 0) bus_.publish(SecurityLoopEnded{});
    target_ = -1;
    float nearest = 48.0f;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        if ((item.control == InteractionControl::Modified && !modified) ||
            (item.control == InteractionControl::Plain && modified))
            continue;
        const float distance =
            std::hypot(item.position.x - world.player.pos.x, item.position.y - world.player.pos.y);
        if (item.canInteract(world.player) && distance <= nearest) {
            const auto& map = world.level.map;
            if (map.tile(static_cast<int>(item.position.x / map.tileSize()),
                         static_cast<int>(item.position.y / map.tileSize())) == TileType::Wall)
                continue;
            // Interaction must not pass through a closed wall; the target door itself is reachable.
            if (!Raycast::hasLineOfSight(world.player.pos, item.position, world.level.map, true))
                continue;
            nearest = distance;
            target_ = static_cast<int>(i);
        }
    }
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (static_cast<int>(i) != target_ || !held) {
            const double before = elapsed_[i];
            elapsed_[i] = items_[i].decayOnLeave ? std::max(0.0, elapsed_[i] - dt / 3.0) : 0;
            if (static_cast<int>(i) != target_ && elapsed_[i] != before)
                bus_.publish(InteractionProgress{
                    items_[i].id, items_[i].holdSeconds > 0
                                      ? static_cast<float>(elapsed_[i] / items_[i].holdSeconds)
                                      : 0});
        }
    }
    if (target_ < 0) return;
    claimedThisTick_ = true;
    auto& item = items_[static_cast<std::size_t>(target_)];
    auto& elapsed = elapsed_[static_cast<std::size_t>(target_)];
    if (held) {
        if (item.onHold)
            item.onHold(static_cast<float>(
                std::min(static_cast<double>(dt), std::max(0.0, item.holdSeconds - elapsed))));
        elapsed += dt;
        if (world.level.map.tile(static_cast<int>(item.position.x / world.level.map.tileSize()),
                                 static_cast<int>(item.position.y / world.level.map.tileSize())) ==
            TileType::ServiceDoor)
            bus_.publish(NoiseEmitted{item.position, lockpickNoise_, NoiseType::Lockpick, "Ghost"});
    }
    bus_.publish(InteractionProgress{item.id, progress()});
    if (held && elapsed >= item.holdSeconds) {
        const auto id = item.id;
        item.onComplete(world);
        elapsed = 0;
        target_ = -1;
        bus_.publish(InteractionDone{id});
    }
}

float InteractionSystem::progress() const {
    if (target_ < 0) return 0;
    const auto i = static_cast<std::size_t>(target_);
    return items_[i].holdSeconds > 0
               ? static_cast<float>(std::clamp(elapsed_[i] / items_[i].holdSeconds, 0.0, 1.0))
               : 0;
}
const Interactable* InteractionSystem::target() const {
    return target_ < 0 ? nullptr : &items_[static_cast<std::size_t>(target_)];
}
bool InteractionSystem::targetAvailable(const World& world) const {
    return target() && target()->canInteract(world.player);
}
