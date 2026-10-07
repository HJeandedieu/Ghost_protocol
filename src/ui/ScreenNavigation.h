#pragma once
#include <array>
#include <functional>

#include "core/Input.h"
#include "ui/Widgets.h"

// Shared keyboard/mouse selection; activating returns a row once per input edge.
class ScreenNavigation {
   public:
    int update(const Input& input, float dt, float hoverTime, int count, float y) {
        focus_ = (focus_ + input.menuVertical + count) % count;
        const bool moved = input.mouseLogical.x != pointer_.x || input.mouseLogical.y != pointer_.y;
        int action = -1;
        for (int i = 0; i < count; ++i) {
            const auto b = bounds(i, y);
            if (moved && b.contains(input.mouseLogical, input.mouseInViewport)) focus_ = i;
            buttons_[i].update(dt, focus_ == i, hoverTime);
            if (buttons_[i].activated(b, input.mouseLogical, input.mouseInViewport,
                                      input.startClicked, focus_ == i, input.confirmPressed))
                action = i;
        }
        pointer_ = input.mouseLogical;
        return action;
    }
    static WidgetBounds bounds(int row, float y) { return {96, y + row * 72.f, 352, 56}; }
    float emphasis(int row) const { return buttons_[row].emphasis(); }

   private:
    int focus_ = 0;
    Vec2 pointer_{-1, -1};
    std::array<Button, 4> buttons_{};
};
