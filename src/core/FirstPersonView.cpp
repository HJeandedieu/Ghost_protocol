#include "core/FirstPersonView.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kRadians = 3.14159265358979323846f / 180;
}

FirstPersonView::FirstPersonView(ViewConfig config, float yawDeg) : config_(config) {
    if (std::isfinite(yawDeg)) yawDeg_ = std::remainder(yawDeg, 360.f);
}

void FirstPersonView::look(Vec2 delta) {
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y)) return;
    // Double arithmetic avoids overflow from large finite input deltas.
    yawDeg_ = static_cast<float>(std::remainder(
        static_cast<double>(yawDeg_) + static_cast<double>(delta.x) * config_.mouseDegPerPx,
        360.0));
    pitchDeg_ = static_cast<float>(std::clamp(
        static_cast<double>(pitchDeg_) + static_cast<double>(delta.y) * config_.mouseDegPerPx,
        -static_cast<double>(config_.pitchLimitDeg), static_cast<double>(config_.pitchLimitDeg)));
}

Vec2 FirstPersonView::movement(Vec2 local) const {
    if (!std::isfinite(local.x) || !std::isfinite(local.y)) return {};
    const double length = std::hypot(static_cast<double>(local.x), static_cast<double>(local.y));
    const double divisor = std::max(1.0, length);
    const float right = static_cast<float>(local.x / divisor);
    const float forward = static_cast<float>(-local.y / divisor);
    const float angle = yawDeg_ * kRadians;
    return {std::cos(angle) * forward - std::sin(angle) * right,
            std::sin(angle) * forward + std::cos(angle) * right};
}

float FirstPersonView::eyeHeight(bool crouched) const {
    return crouched ? config_.crouchEyeHeight : config_.eyeHeight;
}
