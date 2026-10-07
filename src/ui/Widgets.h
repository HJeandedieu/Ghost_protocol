#pragma once
#include <algorithm>
#include <cmath>

#include "core/Vec2.h"

// Pure presentation math: reusable without a window or raylib.
struct WidgetBounds {
    float x, y, width, height;
    bool contains(Vec2 p, bool inViewport) const {
        return inViewport && p.x >= x && p.y >= y && p.x < x + width && p.y < y + height;
    }
};
class Button {
   public:
    void update(float dt, bool selected, float duration) {
        if (!std::isfinite(dt) || dt < 0 || !std::isfinite(duration) || duration <= 0) return;
        progress_ = std::clamp(progress_ + (selected ? dt : -dt) / duration, 0.f, 1.f);
    }
    float emphasis() const {
        const float remaining = 1 - progress_;
        return 1 - remaining * remaining * remaining;
    }
    bool activated(const WidgetBounds& bounds, Vec2 pointer, bool inViewport, bool clicked,
                   bool focused, bool confirm) const {
        return (clicked && bounds.contains(pointer, inViewport)) || (focused && confirm);
    }

   private:
    float progress_ = 0;
};
inline float sliderValue(float pointerX, float left, float width, float previous) {
    if (!std::isfinite(pointerX) || !std::isfinite(width) || width <= 0) return previous;
    return std::clamp((pointerX - left) / width, 0.f, 1.f);
}
inline float transitionProgress(float elapsed, float duration) {
    if (!std::isfinite(elapsed) || !std::isfinite(duration) || duration <= 0) return 1;
    const float t = std::clamp(elapsed / duration, 0.f, 1.f);
    const float remaining = 1 - t;
    return 1 - remaining * remaining * remaining;
}
