#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "core/Vec3.h"

// Conservative x/z projection of the complete unshaken perspective frustum.
// Visibility deliberately ignores walls: an occluded entry in this footprint still waits.
struct ViewFootprint {
    std::array<Vec2, 8> points{};
    int count = 0;
    bool intersects(Vec2 center, float radius) const {
        if (count < 3 || !std::isfinite(center.x) || !std::isfinite(center.y) ||
            !std::isfinite(radius) || radius < 0)
            return true;
        bool inside = true;
        for (int i = 0; i < count; ++i) {
            const auto a = points[i], b = points[(i + 1) % count];
            const double dx = static_cast<double>(b.x) - a.x, dy = static_cast<double>(b.y) - a.y;
            const double x = static_cast<double>(center.x) - a.x,
                         y = static_cast<double>(center.y) - a.y;
            if (dx * y - dy * x < 0) inside = false;
            const double length = dx * dx + dy * dy;
            const double t = length > 0 ? std::clamp((x * dx + y * dy) / length, 0.0, 1.0) : 0;
            const double ex = x - t * dx, ey = y - t * dy;
            if (ex * ex + ey * ey <= static_cast<double>(radius) * radius) return true;
        }
        return inside;
    }
    static ViewFootprint perspective(ShotRay view, float fovYDeg, float aspect, float nearClip,
                                     float farClip) {
        ViewFootprint result;
        const auto ray = view.normalized();
        if (!ray || !std::isfinite(fovYDeg) || fovYDeg <= 0 || fovYDeg >= 180 ||
            !std::isfinite(aspect) || aspect <= 0 || !std::isfinite(nearClip) || nearClip <= 0 ||
            !std::isfinite(farClip) || farClip <= nearClip)
            return result;
        const auto horizontal = ray->direction.cross({0, 1, 0}).normalized();
        // View pitch is clamped short of vertical. Keep a finite basis for callers at vertical.
        const Vec3 right = horizontal ? *horizontal : Vec3{1, 0, 0};
        const Vec3 up = *right.cross(ray->direction).normalized();
        const float tangent = std::tan(fovYDeg * .008726646259971648f);
        std::array<Vec2, 8> corners{};
        int index = 0;
        for (float distance : {nearClip, farClip})
            for (float x : {-1.f, 1.f})
                for (float y : {-1.f, 1.f}) {
                    const auto point = ray->at(distance) +
                                       right * (x * distance * tangent * aspect) +
                                       up * (y * distance * tangent);
                    if (!point.finite()) return {};
                    corners[index++] = point.planar();
                }
        std::sort(corners.begin(), corners.end(),
                  [](Vec2 a, Vec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
        const auto turn = [](Vec2 a, Vec2 b, Vec2 c) {
            return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y) -
                   (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
        };
        std::array<Vec2, 16> hull{};
        int n = 0;
        for (auto point : corners) {
            while (n >= 2 && turn(hull[n - 2], hull[n - 1], point) <= 0) --n;
            hull[n++] = point;
        }
        const int lower = n + 1;
        for (int i = 6; i >= 0; --i) {
            while (n >= lower && turn(hull[n - 2], hull[n - 1], corners[i]) <= 0) --n;
            hull[n++] = corners[i];
        }
        result.count = n - 1;
        for (int i = 0; i < result.count; ++i) result.points[i] = hull[i];
        return result;
    }
};
