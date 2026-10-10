#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "world/Raycast.h"
#include "world/TileMap.h"

struct VisibilityTriangle {
    Vec2 a, b, c;
};

// Render-only planar footprint, using the same supercover raycast as detection/reveal.
inline std::vector<Vec2> visibilityBoundary(Vec2 origin, float facing, float halfAngle, float range,
                                            const TileMap& map) {
    std::vector<Vec2> result;
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(facing) ||
        !std::isfinite(halfAngle) || !std::isfinite(range) || range <= 0 || halfAngle <= 0)
        return result;
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<float> angles;
    constexpr int kSegments = 128;
    for (int i = 0; i <= kSegments; ++i)
        angles.push_back(-halfAngle + 2 * halfAngle * static_cast<float>(i) / kSegments);
    const float size = static_cast<float>(map.tileSize());
    const int left = std::max(0, static_cast<int>(std::floor((origin.x - range) / size)));
    const int right =
        std::min(map.width(), static_cast<int>(std::floor((origin.x + range) / size)) + 1);
    const int top = std::max(0, static_cast<int>(std::floor((origin.y - range) / size)));
    const int bottom =
        std::min(map.height(), static_cast<int>(std::floor((origin.y + range) / size)) + 1);
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x) {
            if (!map.blocksSight(x, y) && !map.blocksSight(x - 1, y) &&
                !map.blocksSight(x, y - 1) && !map.blocksSight(x - 1, y - 1))
                continue;
            const float dx = x * size - origin.x, dy = y * size - origin.y;
            if (std::hypot(dx, dy) > range) continue;
            const float angle = std::remainder(std::atan2(dy, dx) - facing, 2 * kPi);
            for (float offset : {-.00001f, 0.f, .00001f})
                if (angle + offset > -halfAngle && angle + offset < halfAngle)
                    angles.push_back(angle + offset);
        }
    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end()), angles.end());
    for (float angle : angles) {
        const Vec2 dir{std::cos(facing + angle), std::sin(facing + angle)};
        const float distance = Raycast::sightDistance(
            origin, {origin.x + dir.x * range, origin.y + dir.y * range}, map);
        result.push_back({origin.x + dir.x * distance, origin.y + dir.y * distance});
    }
    return result;
}

inline std::vector<VisibilityTriangle> visibilityFan(Vec2 origin, float facing, float halfAngle,
                                                     std::array<float, 3> ranges,
                                                     const TileMap& map) {
    std::vector<VisibilityTriangle> result;
    for (float value : ranges)
        if (!std::isfinite(value) || value <= 0) return result;
    const float range = std::max({ranges[0], ranges[1], ranges[2]});
    const auto boundary = visibilityBoundary(origin, facing, halfAngle, range, map);
    if (boundary.empty()) return result;
    const float size = static_cast<float>(map.tileSize());
    const int left = std::max(0, static_cast<int>((origin.x - range) / size));
    const int right = std::min(map.width() - 1, static_cast<int>((origin.x + range) / size));
    const int top = std::max(0, static_cast<int>((origin.y - range) / size));
    const int bottom = std::min(map.height() - 1, static_cast<int>((origin.y + range) / size));
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x) {
            if (map.blocksSight(x, y)) continue;
            const float tileRange = ranges[map.light(x, y) == LightLevel::Lit   ? 0
                                           : map.light(x, y) == LightLevel::Dim ? 1
                                                                                : 2];
            for (std::size_t i = 1; i < boundary.size(); ++i) {
                const auto limited = [&](Vec2 p) {
                    const float dx = p.x - origin.x, dy = p.y - origin.y;
                    const float distance = std::hypot(dx, dy);
                    const float scale = distance > tileRange ? tileRange / distance : 1;
                    return Vec2{origin.x + dx * scale, origin.y + dy * scale};
                };
                const auto a = limited(boundary[i]), b = limited(boundary[i - 1]);
                if (std::max({origin.x, a.x, b.x}) < x * size ||
                    std::min({origin.x, a.x, b.x}) > (x + 1) * size ||
                    std::max({origin.y, a.y, b.y}) < y * size ||
                    std::min({origin.y, a.y, b.y}) > (y + 1) * size)
                    continue;
                std::array<Vec2, 8> polygon{origin, a, b};
                int count = 3;
                for (int edge = 0; edge < 4 && count >= 3; ++edge) {
                    std::array<Vec2, 8> output{};
                    int n = 0;
                    const bool horizontal = edge < 2;
                    const float limit = horizontal ? (x + (edge == 1 ? 1 : 0)) * size
                                                   : (y + (edge == 3 ? 1 : 0)) * size;
                    const auto coordinate = [&](Vec2 p) { return horizontal ? p.x : p.y; };
                    const auto inside = [&](Vec2 p) {
                        return edge % 2 == 0 ? coordinate(p) >= limit : coordinate(p) <= limit;
                    };
                    for (int j = 0; j < count; ++j) {
                        const auto p = polygon[j], q = polygon[(j + 1) % count];
                        if (inside(p) != inside(q)) {
                            const float t =
                                (limit - coordinate(p)) / (coordinate(q) - coordinate(p));
                            output[n++] = {p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t};
                        }
                        if (inside(q)) output[n++] = q;
                    }
                    polygon = output;
                    count = n;
                }
                for (int j = 1; j + 1 < count; ++j)
                    result.push_back({polygon[0], polygon[j], polygon[j + 1]});
            }
        }
    return result;
}
