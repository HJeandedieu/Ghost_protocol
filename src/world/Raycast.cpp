#include "world/Raycast.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "world/TileMap.h"

bool Raycast::hasLineOfSight(Vec2 from, Vec2 to, const TileMap& map, bool revealBlockingTarget) {
    const float size = static_cast<float>(map.tileSize());
    if (size <= 0 || !std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) ||
        !std::isfinite(to.y))
        return false;
    int x = static_cast<int>(std::floor(from.x / size));
    int y = static_cast<int>(std::floor(from.y / size));
    const int endX = static_cast<int>(std::floor(to.x / size));
    const int endY = static_cast<int>(std::floor(to.y / size));
    if (!map.contains(x, y) || !map.contains(endX, endY) || map.blocksSight(x, y)) return false;
    const float dx = to.x - from.x, dy = to.y - from.y;
    const int stepX = dx > 0 ? 1 : -1, stepY = dy > 0 ? 1 : -1;
    const float infinity = std::numeric_limits<float>::infinity();
    const float deltaX = dx == 0 ? infinity : size / std::abs(dx);
    const float deltaY = dy == 0 ? infinity : size / std::abs(dy);
    float nextX = dx == 0 ? infinity : ((x + (dx > 0 ? 1 : 0)) * size - from.x) / dx;
    float nextY = dy == 0 ? infinity : ((y + (dy > 0 ? 1 : 0)) * size - from.y) / dy;
    while (x != endX || y != endY) {
        if (nextX == nextY) {
            // Supercover: a diagonal cannot leak through two touching wall corners.
            if (map.blocksSight(x + stepX, y) || map.blocksSight(x, y + stepY)) return false;
            x += stepX;
            y += stepY;
            nextX += deltaX;
            nextY += deltaY;
        } else if (nextX < nextY) {
            x += stepX;
            nextX += deltaX;
        } else {
            y += stepY;
            nextY += deltaY;
        }
        if (!map.contains(x, y)) return false;
        if (map.blocksSight(x, y)) return revealBlockingTarget && x == endX && y == endY;
    }
    return true;
}

std::optional<float> Raycast::intersectCircle(Vec2 from, Vec2 to, Vec2 center, float radius) {
    if (!std::isfinite(radius) || radius < 0) return std::nullopt;
    const Vec2 direction{to.x - from.x, to.y - from.y};
    const Vec2 offset{from.x - center.x, from.y - center.y};
    const float c = offset.x * offset.x + offset.y * offset.y - radius * radius;
    if (c <= 0) return 0.0f;
    const float a = direction.x * direction.x + direction.y * direction.y;
    if (a == 0) return std::nullopt;
    const float b = offset.x * direction.x + offset.y * direction.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0) return std::nullopt;
    const float t = (-b - std::sqrt(discriminant)) / a;
    if (t >= 0 && t <= 1) return t;
    return std::nullopt;
}
