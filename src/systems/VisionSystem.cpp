#include "systems/VisionSystem.h"

#include <cmath>

#include "core/EventBus.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "world/Raycast.h"

bool VisionSystem::sees(const Vec2& eye, float facingDeg, float halfAngleDeg, float range,
                        const Vec2& target, const TileMap& map) const {
    if (!std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(target.x) ||
        !std::isfinite(target.y) || !std::isfinite(facingDeg) || !std::isfinite(halfAngleDeg) ||
        !std::isfinite(range) || range < 0 || halfAngleDeg < 0 || halfAngleDeg > 180)
        return false;
    const float dx = target.x - eye.x, dy = target.y - eye.y;
    const float distance = std::hypot(dx, dy);
    if (distance > range) return false;
    if (distance > 0) {
        constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;
        const float difference = std::remainder(
            std::atan2(dy, dx) * kRadiansToDegrees - std::remainder(facingDeg, 360.0f), 360.0f);
        // Allow only floating-point roundoff at the inclusive cone boundary.
        if (std::abs(difference) > halfAngleDeg + 0.00001f) return false;
    }
    return Raycast::hasLineOfSight(eye, target, map);
}

float VisionSystem::rangeFor(LightLevel light, bool crouched) const {
    if (light == LightLevel::Lit) return config_.rangeLit;
    if (light == LightLevel::Dim) return config_.rangeDim;
    return config_.rangeDark * (crouched ? config_.crouchDarkMult : 1.0f);
}

bool VisionSystem::sees(const Guard& guard, const Player& player, const TileMap& map) const {
    if (guard.state() == GuardState::Unconscious) return false;
    if (!std::isfinite(player.pos.x) || !std::isfinite(player.pos.y) || map.tileSize() <= 0)
        return false;
    const int x = static_cast<int>(std::floor(player.pos.x / map.tileSize()));
    const int y = static_cast<int>(std::floor(player.pos.y / map.tileSize()));
    if (!map.contains(x, y)) return false;
    constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;
    return sees(guard.pos, guard.facing() * kRadiansToDegrees, config_.coneDeg * 0.5f,
                rangeFor(map.light(x, y), player.isCrouched()), player.pos, map);
}

void VisionSystem::findBodies(const std::vector<Guard>& guards, const TileMap& map,
                              EventBus& events) const {
    constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;
    for (const auto& finder : guards) {
        if (finder.state() == GuardState::Unconscious || finder.state() == GuardState::Combat ||
            finder.state() == GuardState::Alerted)
            continue;
        const VisionSystem vision(finder.visionConfig());
        for (const auto& body : guards) {
            if (body.state() != GuardState::Unconscious || !std::isfinite(body.pos.x) ||
                !std::isfinite(body.pos.y) || map.tileSize() <= 0)
                continue;
            const int x = static_cast<int>(std::floor(body.pos.x / map.tileSize()));
            const int y = static_cast<int>(std::floor(body.pos.y / map.tileSize()));
            if (!map.contains(x, y)) continue;
            if (vision.sees(finder.pos, finder.facing() * kRadiansToDegrees,
                            finder.visionConfig().coneDeg * 0.5f,
                            vision.rangeFor(map.light(x, y), false), body.pos, map)) {
                events.publish(BodyFound{finder.id, body.id});
                break;
            }
        }
    }
}
