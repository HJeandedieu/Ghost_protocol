#pragma once

#include <cmath>
#include <optional>

#include "core/Vec2.h"

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(Vec3 other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    float dot(Vec3 other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3 cross(Vec3 other) const {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }
    bool finite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
    std::optional<Vec3> normalized() const {
        const double length =
            std::hypot(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z));
        if (!finite() || !std::isfinite(length) || length == 0) return std::nullopt;
        return Vec3{static_cast<float>(x / length), static_cast<float>(y / length),
                    static_cast<float>(z / length)};
    }
    Vec2 planar() const { return {x, z}; }
};

struct ShotRay {
    Vec3 origin;
    Vec3 direction;
    std::optional<ShotRay> normalized() const {
        const auto unit = direction.normalized();
        if (!origin.finite() || !unit) return std::nullopt;
        return ShotRay{origin, *unit};
    }
    Vec3 at(float distance) const { return origin + direction * distance; }
    static ShotRay aim(Vec2 position, float height, float yawDeg, float pitchDeg = 0) {
        constexpr float kRadians = 3.14159265358979323846f / 180;
        const float yaw = yawDeg * kRadians, pitch = pitchDeg * kRadians;
        return {
            {position.x, height, position.y},
            {std::cos(pitch) * std::cos(yaw), -std::sin(pitch), std::cos(pitch) * std::sin(yaw)}};
    }
};
