#include "world/Raycast.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "world/TileMap.h"

std::optional<float> Raycast::intersectPrism(ShotRay ray, Vec3 minimum, Vec3 maximum) {
    const auto unit = ray.normalized();
    if (!unit || !minimum.finite() || !maximum.finite()) return std::nullopt;
    const float origins[] = {unit->origin.x, unit->origin.y, unit->origin.z};
    const float directions[] = {unit->direction.x, unit->direction.y, unit->direction.z};
    const float low[] = {minimum.x, minimum.y, minimum.z};
    const float high[] = {maximum.x, maximum.y, maximum.z};
    double entry = 0, exit = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        if (low[axis] > high[axis]) return std::nullopt;
        if (directions[axis] == 0) {
            if (origins[axis] < low[axis] || origins[axis] > high[axis]) return std::nullopt;
            continue;
        }
        double a = (static_cast<double>(low[axis]) - origins[axis]) / directions[axis];
        double b = (static_cast<double>(high[axis]) - origins[axis]) / directions[axis];
        if (a > b) std::swap(a, b);
        entry = std::max(entry, a);
        exit = std::min(exit, b);
        if (exit < entry) return std::nullopt;
    }
    return static_cast<float>(entry);
}

std::optional<float> Raycast::intersectCylinder(ShotRay ray, Vec2 center, float radius,
                                                float height) {
    const auto unit = ray.normalized();
    if (!unit || !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(radius) ||
        radius <= 0 || !std::isfinite(height) || height <= 0)
        return std::nullopt;
    ray = *unit;
    const double x = static_cast<double>(ray.origin.x) - center.x;
    const double z = static_cast<double>(ray.origin.z) - center.y;
    const double radiusSquared = static_cast<double>(radius) * radius;
    if (x * x + z * z <= radiusSquared && ray.origin.y >= 0 && ray.origin.y <= height) return 0;
    double nearest = std::numeric_limits<double>::infinity();
    const double a = static_cast<double>(ray.direction.x) * ray.direction.x +
                     static_cast<double>(ray.direction.z) * ray.direction.z;
    const double b = 2 * (x * ray.direction.x + z * ray.direction.z);
    const double c = x * x + z * z - radiusSquared;
    const double discriminant = b * b - 4 * a * c;
    if (a > 0 && discriminant >= 0) {
        for (double distance :
             {(-b - std::sqrt(discriminant)) / (2 * a), (-b + std::sqrt(discriminant)) / (2 * a)}) {
            const double y = ray.origin.y + distance * ray.direction.y;
            if (distance >= 0 && y >= 0 && y <= height) nearest = std::min(nearest, distance);
        }
    }
    if (ray.direction.y != 0) {
        for (double y : {0.0, static_cast<double>(height)}) {
            const double distance = (y - ray.origin.y) / ray.direction.y;
            const double dx = x + distance * ray.direction.x;
            const double dz = z + distance * ray.direction.z;
            if (distance >= 0 && dx * dx + dz * dz <= radiusSquared)
                nearest = std::min(nearest, distance);
        }
    }
    return std::isfinite(nearest) ? std::optional<float>(static_cast<float>(nearest))
                                  : std::nullopt;
}

std::optional<float> Raycast::intersectFloor(ShotRay ray) {
    const auto unit = ray.normalized();
    if (!unit) return std::nullopt;
    if (unit->origin.y <= 0) return 0;
    if (unit->direction.y >= 0) return std::nullopt;
    return -unit->origin.y / unit->direction.y;
}

std::optional<float> Raycast::intersectShield(ShotRay ray, Vec2 center, float facing,
                                              const ShotGeometryConfig& geometry) {
    const auto unit = ray.normalized();
    if (!unit || !std::isfinite(facing) || !std::isfinite(center.x) || !std::isfinite(center.y))
        return std::nullopt;
    ray = *unit;
    const Vec3 normal{std::cos(facing), 0, std::sin(facing)};
    const Vec3 across{-normal.z, 0, normal.x};
    const Vec3 position{center.x + normal.x * geometry.shieldForwardOffset, 0,
                        center.y + normal.z * geometry.shieldForwardOffset};
    const auto offset = ray.origin - position;
    const ShotRay local{{offset.dot(across), offset.y, offset.dot(normal)},
                        {ray.direction.dot(across), ray.direction.y, ray.direction.dot(normal)}};
    return intersectPrism(
        local, {-geometry.shieldWidth * .5f, geometry.shieldBottom, 0},
        {geometry.shieldWidth * .5f, geometry.shieldBottom + geometry.shieldHeight, 0});
}

std::optional<float> Raycast::blockingDistance(ShotRay ray, float range, const TileMap& map,
                                               float wallHeight) {
    const auto unit = ray.normalized();
    if (!unit || !std::isfinite(range) || range < 0 || !std::isfinite(wallHeight) ||
        wallHeight <= 0 || map.tileSize() <= 0)
        return std::nullopt;
    ray = *unit;
    std::optional<float> nearest;
    if (const auto floor = intersectFloor(ray); floor && *floor <= range) nearest = floor;
    const float size = static_cast<float>(map.tileSize());
    // Only test tile prisms overlapping the planar segment's bounding box.
    const auto end = ray.at(nearest.value_or(range));
    const auto index = [size](float coordinate, int maximum) {
        return static_cast<int>(std::clamp(std::floor(static_cast<double>(coordinate) / size), 0.0,
                                           static_cast<double>(maximum)));
    };
    const int x0 = index(std::min(ray.origin.x, end.x) - size, map.width() - 1);
    const int x1 = index(std::max(ray.origin.x, end.x), map.width() - 1);
    const int z0 = index(std::min(ray.origin.z, end.z) - size, map.height() - 1);
    const int z1 = index(std::max(ray.origin.z, end.z), map.height() - 1);
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
            if (!map.blocksSight(x, z)) continue;
            const auto hit = intersectPrism(ray, {x * size, 0, z * size},
                                            {(x + 1) * size, wallHeight, (z + 1) * size});
            if (hit && *hit <= range && (!nearest || *hit < *nearest)) nearest = hit;
        }
    return nearest;
}

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
