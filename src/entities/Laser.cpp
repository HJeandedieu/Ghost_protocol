#include "entities/Laser.h"

#include <algorithm>

Laser::Laser(const LaserSpawn& spawn, const TileMap& map) {
    id = spawn.id;
    pos = prevPos = map.tileCenter(spawn.a);
    end_ = map.tileCenter(spawn.b);
}

Vec2 Laser::nearestPoint(Vec2 point) const {
    const Vec2 delta{end_.x - pos.x, end_.y - pos.y};
    const float lengthSquared = delta.x * delta.x + delta.y * delta.y;
    const float fraction =
        lengthSquared > 0 ? std::clamp(((point.x - pos.x) * delta.x + (point.y - pos.y) * delta.y) /
                                           lengthSquared,
                                       0.0f, 1.0f)
                          : 0;
    return {pos.x + delta.x * fraction, pos.y + delta.y * fraction};
}
