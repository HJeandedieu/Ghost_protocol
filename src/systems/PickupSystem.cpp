#include "systems/PickupSystem.h"

#include <algorithm>
#include <cmath>

#include "core/EventBus.h"
#include "systems/CombatSystem.h"
#include "world/Raycast.h"
#include "world/World.h"

PickupSystem::PickupSystem(EventBus& events, const PickupConfig& config, std::uint32_t seed)
    : events_(events), config_(config), rng_(seed) {}

void PickupSystem::spawn(PickupType type, Vec2 position, World& world) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) return;
    world.pickups.push_back(
        std::make_unique<RecoveryPickup>("pickup:" + std::to_string(nextId_++), type, position));
}

void PickupSystem::dropForPolice(const std::string& sourceId, const std::string& type,
                                 Vec2 position, World& world) {
    if (type != "cop" && type != "shield_cop" && type != "heavy") return;
    if (sourceId.empty() || !std::isfinite(position.x) || !std::isfinite(position.y) ||
        !rolledDeaths_.insert(sourceId).second)
        return;
    const float roll = rng_.uniformFloat(0, 1);
    if (roll < config_.medkitChance)
        spawn(PickupType::Medkit, position, world);
    else if (roll < config_.medkitChance + config_.armorChance)
        spawn(PickupType::ArmorPlate, position, world);
}

const RecoveryPickup* PickupSystem::target(const World& world, bool interactionClaimed) const {
    if (interactionClaimed || world.player.dead()) return nullptr;
    const RecoveryPickup* nearest = nullptr;
    float nearestDistance = config_.collectRadius;
    for (const auto& pickup : world.pickups) {
        const bool medkit = pickup->type == PickupType::Medkit;
        if (medkit
                ? (config_.medkitAmount <= 0 || world.player.hp() >= world.player.maximumHp())
                : (config_.armorAmount <= 0 || world.player.armor() >= world.player.maximumArmor()))
            continue;
        const float distance =
            std::hypot(pickup->pos.x - world.player.pos.x, pickup->pos.y - world.player.pos.y);
        if (distance > nearestDistance || (nearest && distance == nearestDistance)) continue;
        if (!Raycast::hasLineOfSight(world.player.pos, pickup->pos, world.level.map)) continue;
        nearest = pickup.get();
        nearestDistance = distance;
    }
    return nearest;
}

void PickupSystem::update(bool pressed, bool interactionClaimed, World& world,
                          CombatSystem& combat) {
    if (!pressed) return;
    const auto* selected = target(world, interactionClaimed);
    if (!selected) return;
    const float amount =
        selected->type == PickupType::Medkit ? config_.medkitAmount : config_.armorAmount;
    if (combat.restore(world.player, selected->type, amount) <= 0) return;
    const auto id = selected->id;
    world.pickups.erase(
        std::remove_if(world.pickups.begin(), world.pickups.end(),
                       [selected](const auto& pickup) { return pickup.get() == selected; }),
        world.pickups.end());
    events_.publish(InteractionDone{id});
}
