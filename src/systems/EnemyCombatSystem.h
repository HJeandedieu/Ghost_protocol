#pragma once
#include <unordered_map>

#include "core/Config.h"
#include "core/Rng.h"
#include "core/Vec2.h"
#include "core/Vec3.h"
#include "entities/EnemySpec.h"
class EventBus;
class CombatSystem;
class PickupSystem;
class Entity;
struct World;
struct EnemyShot {
    Vec2 from, to;
    float age = 0;
    Vec3 from3D, to3D;
    bool hitPlayer = false;
};
class EnemyCombatSystem {
   public:
    EnemyCombatSystem(EventBus&, World&, CombatSystem&, PickupSystem&,
                      const std::vector<EnemySpec>&, const Config&, std::uint32_t seed,
                      float damageMultiplier = -1);
    void update(float dt);
    bool spawnDebugCop();
    const std::vector<EnemyShot>& shots() const { return shots_; }

   private:
    struct Actor {
        bool acquired = false;
        double elapsed = 0, nextBullet = 0, burstStart = 0, strafeTime = 0;
        int bullet = 0;
        float pathTime = 0;
        std::vector<Vec2> path;
        std::size_t waypoint = 0;
    };
    EventBus& events_;
    World& world_;
    CombatSystem& combat_;
    const Config& config_;
    float damageMultiplier_;
    std::vector<EnemySpec> specs_;
    Rng rng_;
    std::unordered_map<std::string, Actor> actors_;
    std::vector<EnemyShot> shots_;
    std::size_t nextCop_ = 0;
    void updateActor(Entity&, float& facing, const EnemySpec&, float speed, bool strafe, float dt);
    void moveToward(Entity&, Actor&, float speed, float dt);
};
