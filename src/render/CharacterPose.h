#pragma once

#include <algorithm>
#include <cmath>

// Presentation samples only. These values never affect aim, collision or shot volumes.
struct CharacterPose {
    float leftLeg = 0, rightLeg = 0, leftKnee = 0, rightKnee = 0;
    static CharacterPose walking(float travel, float height, bool moving) {
        if (!moving || !std::isfinite(travel) || !std::isfinite(height) || height <= 0) return {};
        const float stride = std::sin(travel / height * 8);
        return {stride * .35f, -stride * .35f, std::max(0.f, -stride) * .42f,
                std::max(0.f, stride) * .42f};
    }
};

struct WeaponPose {
    float lower = 0, turn = 0;
    static WeaponPose reload(float remaining, float duration) {
        if (!std::isfinite(remaining) || !std::isfinite(duration) || duration <= 0 ||
            remaining <= 0)
            return {};
        const float progress = 1 - std::clamp(remaining / duration, 0.f, 1.f);
        const float envelope = std::sin(progress * 3.14159265358979323846f);
        return {envelope * .22f, envelope * -.5f};
    }
};
