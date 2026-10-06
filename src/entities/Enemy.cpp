#include "entities/Enemy.h"

#include <stdexcept>
#include <utility>
Enemy::Enemy(std::string entityId, Vec2 position, EnemySpec spec, float collisionRadius)
    : spec_(std::move(spec)) {
    id = std::move(entityId);
    pos = prevPos = position;
    radius = collisionRadius;
    initializeVitals(spec_.hp, spec_.armor);
}
Cop::Cop(std::string entityId, Vec2 position, const EnemySpec& spec, float radius)
    : Enemy(std::move(entityId), position, spec, radius) {
    if (spec.id != "cop") throw std::invalid_argument("Cop requires cop tuning");
}
