#include "world/Raycast.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "world/TileMap.h"

bool Raycast::isPathClear(Vec2 from, Vec2 to, float radius, const TileMap& map, float step) {
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) ||
        !std::isfinite(to.y) || !std::isfinite(radius) || radius < 0 || !std::isfinite(step) ||
        step <= 0 || map.tileSize() <= 0)
        return false;
    const float distance = std::hypot(to.x - from.x, to.y - from.y);
    if (!std::isfinite(distance)) return false;
    const float size = static_cast<float>(map.tileSize());
    const auto clear = [&](Vec2 point) {
        if (point.x - radius < 0 || point.y - radius < 0 ||
            point.x + radius >= map.width() * size || point.y + radius >= map.height() * size)
            return false;
        const int x0 = static_cast<int>(std::floor((point.x - radius) / size));
        const int x1 = static_cast<int>(std::floor((point.x + radius) / size));
        const int y0 = static_cast<int>(std::floor((point.y - radius) / size));
        const int y1 = static_cast<int>(std::floor((point.y + radius) / size));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                if (map.isPassable(x, y) || map.tile(x, y) == TileType::Door) continue;
                const float dx = point.x - std::clamp(point.x, x * size, (x + 1) * size);
                const float dy = point.y - std::clamp(point.y, y * size, (y + 1) * size);
                if (dx * dx + dy * dy < radius * radius || radius == 0) return false;
            }
        return true;
    };
    if (!clear(from) || !clear(to)) return false;
    for (float travelled = step; travelled < distance; travelled += step) {
        const float fraction = travelled / distance;
        if (!clear({from.x + (to.x - from.x) * fraction, from.y + (to.y - from.y) * fraction}))
            return false;
    }
    return true;
}

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

float Raycast::sightDistance(Vec2 from, Vec2 to, const TileMap& map) {
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) ||
        !std::isfinite(to.y))
        return 0;
    const float size = static_cast<float>(map.tileSize());
    int x = static_cast<int>(std::floor(from.x / size));
    int y = static_cast<int>(std::floor(from.y / size));
    if (!map.contains(x, y) || map.blocksSight(x, y)) return 0;
    const float dx = to.x - from.x, dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    if (length == 0 || !std::isfinite(length)) return 0;
    const int stepX = dx > 0 ? 1 : -1, stepY = dy > 0 ? 1 : -1;
    const float infinity = std::numeric_limits<float>::infinity();
    const float deltaX = dx == 0 ? infinity : size / std::abs(dx);
    const float deltaY = dy == 0 ? infinity : size / std::abs(dy);
    float nextX = dx == 0 ? infinity : ((x + (dx > 0 ? 1 : 0)) * size - from.x) / dx;
    float nextY = dy == 0 ? infinity : ((y + (dy > 0 ? 1 : 0)) * size - from.y) / dy;
    while (true) {
        const float entry = std::min(nextX, nextY);
        if (entry > 1) return length;
        if (nextX == nextY) {
            if (map.blocksSight(x + stepX, y) || map.blocksSight(x, y + stepY))
                return std::max(0.0f, entry * length);
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
        if (map.blocksSight(x, y)) return std::max(0.0f, entry * length);
    }
}
