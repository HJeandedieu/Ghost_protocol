#pragma once

#include <unordered_set>

#include "core/Config.h"
#include "core/Rng.h"
#include "entities/RecoveryPickup.h"

class EventBus;
class CombatSystem;
struct World;

class PickupSystem {
   public:
    PickupSystem(EventBus& events, const PickupConfig& config, std::uint32_t seed);
    // Called once a confirmed police death is known (Day 17 integration).
    void dropForPolice(const std::string& sourceId, const std::string& type, Vec2 position,
                       World& world);
    void spawn(PickupType type, Vec2 position, World& world);
    const RecoveryPickup* target(const World& world, bool interactionClaimed) const;
    void update(bool pressed, bool interactionClaimed, World& world, CombatSystem& combat);

   private:
    EventBus& events_;
    PickupConfig config_;
    Rng rng_;
    std::unordered_set<std::string> rolledDeaths_;
    std::size_t nextId_ = 0;
};
