#include "states/BriefingState.h"

#include <cmath>
#include <utility>

#include "ui/ScreenDrawing.h"
BriefingState::BriefingState(const Input& input, Renderer& renderer, UiConfig config,
                             std::function<void()> start, std::function<void()> back)
    : input_(input),
      renderer_(renderer),
      config_(config),
      start_(std::move(start)),
      back_(std::move(back)) {}
void BriefingState::update(float dt) {
    elapsed_ += dt;
    if (input_.pingPressed) {
        start_();
        return;
    }
    const int action = navigation_.update(input_, dt, config_.hoverTime, 2, 440);
    if (input_.backPressed || action == 1) {
        if (!slide_) {
            back_();
            return;
        }
        renderer_.startTransition(config_.transitionTime, true);
        --slide_;
        elapsed_ = 0;
        return;
    }
    if (action == 0) {
        if (slide_ == 3) {
            start_();
            return;
        }
        renderer_.startTransition(config_.transitionTime);
        ++slide_;
        elapsed_ = 0;
    }
}
void BriefingState::render(float alpha) {
    (void)alpha;
    ClearBackground({10, 10, 12, 255});
    const auto& art = renderer_.uiAssets();
    const float p =
        renderer_.reduceEffects() ? 1 : transitionProgress(elapsed_, config_.transitionTime);
    const float x = 640 + (1 - p) * 96;
    const char* titles[] = {"GOTHAM CENTRAL BANK", "TEN BAGS. ONE NIGHT.", "YOUR WAY OUT",
                            "ARE YOU IN OR OUT?"};
    art.text("MISSION BRIEFING", {96, 72}, 20, Palette::Teal, false, true);
    art.text(TextFormat("%02i / 04", slide_ + 1), {1100, 72}, 20, Palette::Bone);
    art.text(titles[slide_], {96, 184}, 28, Palette::Bone, false, true);
    if (slide_ == 0) {
        DrawTriangleLines({x, 264}, {x + 236, 160}, {x + 472, 264}, Palette::Teal);
        DrawRectangleLinesEx({x, 272, 472, 288}, 3, Palette::Teal);
        for (int i = 0; i < 5; ++i) {
            DrawRectangleRec({x + 24 + i * 92, 296, 20, 240}, Palette::DeepTeal);
            if (i < 4) DrawRectangleLinesEx({x + 52 + i * 92, 320, 48, 112}, 2, Palette::Bone);
        }
        DrawRectangleLinesEx({x - 24, 560, 520, 24}, 2, Palette::Teal);
    } else if (slide_ == 1) {
        DrawRectangleLinesEx({x, 168, 456, 400}, 4, Palette::Teal);
        DrawRectangleLinesEx({x + 24, 192, 408, 352}, 2, Palette::Bone);
        DrawCircleLines(static_cast<int>(x + 228), 328, 96, Palette::Teal);
        for (int i = 0; i < 6; ++i) {
            const float angle = i * 60.f;
            DrawLineEx({x + 228, 328},
                       {x + 228 + 64 * cosf(angle * DEG2RAD), 328 + 64 * sinf(angle * DEG2RAD)}, 4,
                       Palette::Bone);
        }
        for (int i = 0; i < 10; ++i) {
            const float bx = x + 28 + (i % 5) * 80.f;
            const float by = 448 + (i / 5) * 48.f;
            DrawRectangleRounded({bx, by, 60, 36}, .25f, 6, Palette::DeepTeal);
            DrawRectangleLinesEx({bx + 8, by + 4, 44, 28}, 2, {242, 183, 5, 255});
        }
    } else if (slide_ == 2) {
        DrawRectangleRounded({x, 272, 456, 208}, .1f, 8, Palette::DeepTeal);
        DrawRectangleLinesEx({x + 8, 280, 440, 192}, 3, Palette::Teal);
        DrawRectangleRec({x + 304, 296, 112, 72}, {10, 10, 12, 255});
        DrawLineEx({x + 280, 280}, {x + 280, 472}, 3, Palette::Teal);
        DrawCircleV({x + 96, 480}, 40, {10, 10, 12, 255});
        DrawCircleV({x + 368, 480}, 40, {10, 10, 12, 255});
        DrawCircleLines(static_cast<int>(x + 96), 480, 24, Palette::Bone);
        DrawCircleLines(static_cast<int>(x + 368), 480, 24, Palette::Bone);
        DrawLineEx({x - 32, 528}, {x + 488, 528}, 3, Palette::Teal);
    } else {
        renderer_.drawGhostPortrait({x + 96, 184, 288, 344});
        DrawRectangleLinesEx({x + 96, 184, 288, 344}, 3, Palette::Teal);
        art.text("GHOST", {x + 112, 494}, 22, Palette::Bone, false, true);
    }
    drawScreenButton(renderer_, navigation_, 0, 440, slide_ == 3 ? "START HEIST" : "NEXT");
    drawScreenButton(renderer_, navigation_, 1, 440, "BACK");
    art.text("SPACE skip briefing   ESC back", {96, 572}, 14, Palette::Teal, true);
}
