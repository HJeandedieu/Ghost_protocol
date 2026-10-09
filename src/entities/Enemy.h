#pragma once
#include "entities/EnemySpec.h"
#include "entities/Entity.h"

enum class EnemyState { Advance, Engage, Dead };
class Enemy : public Entity {
   public:
    Enemy(std::string entityId, Vec2 position, EnemySpec spec, float collisionRadius);
    const EnemySpec& spec() const { return spec_; }
    EnemyState state() const { return dead() ? EnemyState::Dead : state_; }
    float facing() const { return facing_; }
    float shieldPlateDamage(float damage, Vec2 from) const;
    bool shieldFaces(Vec2 from) const;

   private:
    friend class EnemyCombatSystem;
    EnemySpec spec_;
    EnemyState state_ = EnemyState::Advance;
    float facing_ = 0;
};
class Cop : public Enemy {
   public:
    Cop(std::string entityId, Vec2 position, const EnemySpec& spec, float radius);
};
class ShieldCop : public Enemy {
   public:
    ShieldCop(std::string entityId, Vec2 position, const EnemySpec& spec);
};
class Heavy : public Enemy {
   public:
    Heavy(std::string entityId, Vec2 position, const EnemySpec& spec);
};
