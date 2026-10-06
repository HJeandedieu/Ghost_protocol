#pragma once
#include <array>
#include <vector>

#include "core/Input.h"
#include "core/Rng.h"
#include "entities/RecoveryPickup.h"
#include "entities/Weapon.h"
class EventBus;
class Entity;
class Player;
struct World;
struct PelletHit {
    Vec2 from;
    Vec2 to;
    float dirDeg = 0;
    std::string targetId;
    float damage = 0;
};
struct HitResult {
    std::vector<PelletHit> pellets;
};
class CombatSystem {
   public:
    CombatSystem(EventBus& events, const std::vector<WeaponSpec>& specs, std::uint32_t seed);
    void update(float dt, const Input& input, float dirDeg, World& world);
    void applyDamage(Entity& target, float amount, const std::string& sourceId);
    void updateHealth(float dt, Player& player);
    float restore(Player& player, PickupType type, float amount);
    HitResult fire(const Weapon& weapon, Vec2 from, float dirDeg, Rng& rng, World& world);
    const Weapon& activeWeapon() const { return weapons_[activeSlot_]; }
    int activeSlot() const { return activeSlot_; }
    const HitResult& lastShot() const { return lastShot_; }
    float shotAge() const { return shotAge_; }

   private:
    EventBus& events_;
    std::array<Weapon, 2> weapons_;
    int activeSlot_ = 0;
    Rng rng_;
    HitResult lastShot_;
    float shotAge_ = 1;
};
