#include "render/FollowCamera.h"

#include <algorithm>
#include <cmath>

FollowCamera::FollowCamera(Vec2 spawn, const ViewConfig& config)
    : config_(config), target_(spawn), previous_(spawn) {}

void FollowCamera::update(float dt, Vec2 player, Vec2 cursorOffset) {
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    previous_ = target_;
    const float length = std::hypot(cursorOffset.x, cursorOffset.y);
    const float scale = length > config_.leadPx && length > 0.0f ? config_.leadPx / length : 1.0f;
    const Vec2 goal{player.x + cursorOffset.x * scale, player.y + cursorOffset.y * scale};
    const float weight = 1.0f - std::exp(-config_.followRate * dt);
    target_.x += (goal.x - target_.x) * weight;
    target_.y += (goal.y - target_.y) * weight;
}

Vec2 FollowCamera::interpolatedTarget(float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    return {previous_.x + (target_.x - previous_.x) * alpha,
            previous_.y + (target_.y - previous_.y) * alpha};
}
