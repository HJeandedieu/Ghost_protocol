#pragma once
#include <array>
#include <vector>

#include "core/Config.h"
#include "core/Input.h"
#include "core/Rng.h"
#include "core/Vec3.h"
#include "entities/RecoveryPickup.h"
#include "entities/Weapon.h"
class EventBus;
class Entity;
class Player;
struct World;
enum class ShotImpact { None, Geometry, Body, Shield };
struct PelletHit {
    Vec2 from;
    Vec2 to;
    float dirDeg = 0;
    std::string targetId;
    float damage = 0;
    Vec3 from3D, to3D, direction3D;
    ShotImpact impact = ShotImpact::None;
};
struct HitResult {
    std::vector<PelletHit> pellets;
};
class CombatSystem {
   public:
    CombatSystem(EventBus& events, const std::vector<WeaponSpec>& specs, std::uint32_t seed,
                 std::array<std::string, 2> loadout = {{"whisper", "chatter"}},
                 ShotGeometryConfig geometry = {}, float wallHeight = ViewConfig{}.wallHeight);
    void update(float dt, const Input& input, ShotRay ray, World& world);
    void applyDamage(Entity& target, float amount, const std::string& sourceId);
    void updateHealth(float dt, Player& player);
    float restore(Player& player, PickupType type, float amount);
    HitResult fire(const Weapon& weapon, ShotRay ray, Rng& rng, World& world);
    const ShotGeometryConfig& geometry() const { return geometry_; }
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
    ShotGeometryConfig geometry_;
    float wallHeight_;
};
