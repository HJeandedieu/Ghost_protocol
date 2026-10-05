#include "entities/Player.h"

#include <algorithm>
#include <cmath>

#include "world/TileMap.h"

Player::Player(Vec2 spawn, const PlayerConfig& config) : config_(config) {
    pos = prevPos = spawn;
    radius = config.radius;
    id = "Ghost";
}

void Player::update(float dt, const Input& input, const TileMap& map) {
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    prevPos = pos;
    if (input.crouchPressed) crouched_ = !crouched_;
    sprinting_ = input.sprintHeld && !crouched_;
    const float speed = crouched_ ? config_.crouch : (sprinting_ ? config_.sprint : config_.walk);
    const float length = std::hypot(input.move.x, input.move.y);
    const float divisor = std::max(1.0f, length);
    const Vec2 target{input.move.x / divisor * speed, input.move.y / divisor * speed};
    const Vec2 difference{target.x - velocity_.x, target.y - velocity_.y};
    const float distance = std::hypot(difference.x, difference.y);
    const bool slowing =
        length == 0.0f || std::hypot(target.x, target.y) < std::hypot(velocity_.x, velocity_.y);
    const float step = (slowing ? config_.decel : config_.accel) * dt;
    if (distance <= step)
        velocity_ = target;
    else if (distance > 0.0f) {
        velocity_.x += difference.x / distance * step;
        velocity_.y += difference.y / distance * step;
    }
    const Vec2 desired{pos.x + velocity_.x * dt, pos.y + velocity_.y * dt};
    const auto resolved = map.moveCircle(pos, {velocity_.x * dt, velocity_.y * dt}, radius);
    if (resolved.x != desired.x) velocity_.x = 0.0f;
    if (resolved.y != desired.y) velocity_.y = 0.0f;
    pos = resolved;
}

Vec2 Player::interpolatedPosition(float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    return {prevPos.x + (pos.x - prevPos.x) * alpha, prevPos.y + (pos.y - prevPos.y) * alpha};
}
