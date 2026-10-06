#include "entities/Enemy.h"

#include <cmath>
#include <stdexcept>
#include <utility>
Enemy::Enemy(std::string entityId, Vec2 position, EnemySpec spec, float collisionRadius)
    : spec_(std::move(spec)) {
    id = std::move(entityId);
    pos = prevPos = position;
    radius = collisionRadius;
    initializeVitals(spec_.hp, spec_.armor);
}
ShieldCop::ShieldCop(std::string entityId, Vec2 position, const EnemySpec& spec)
    : Enemy(std::move(entityId), position, spec, spec.radius) {
    if (spec.id != "shield_cop") throw std::invalid_argument("ShieldCop requires shield tuning");
}
Heavy::Heavy(std::string entityId, Vec2 position, const EnemySpec& spec)
    : Enemy(std::move(entityId), position, spec, spec.radius) {
    if (spec.id != "heavy") throw std::invalid_argument("Heavy requires heavy tuning");
}
float Enemy::hitscanDamage(float damage, Vec2 from) const {
    if (spec_.id != "shield_cop" || dead()) return damage;
    constexpr float kPi = 3.14159265358979323846f;
    const float angle = std::atan2(from.y - pos.y, from.x - pos.x);
    const float difference = std::abs(std::remainder(angle - facing_, 2 * kPi));
    return difference <= spec_.shieldArcDeg * kPi / 360 + 1e-6f ? damage * (1 - spec_.shieldBlock)
                                                                : damage;
}
Cop::Cop(std::string entityId, Vec2 position, const EnemySpec& spec, float radius)
    : Enemy(std::move(entityId), position, spec, radius) {
    if (spec.id != "cop") throw std::invalid_argument("Cop requires cop tuning");
}
