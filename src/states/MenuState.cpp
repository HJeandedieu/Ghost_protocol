#include "states/MenuState.h"

#include <cmath>
#include <utility>

#include "raylib.h"
#include "render/Renderer.h"
namespace {
constexpr Color kInk{10, 10, 12, 255}, kSlate{20, 22, 27, 255}, kBone{233, 228, 208, 255},
    kTeal{63, 143, 140, 255}, kGold{242, 183, 5, 255}, kGrey{133, 133, 133, 255};
Color mix(Color a, Color b, float t) {
    return {static_cast<unsigned char>(a.r + (b.r - a.r) * t),
            static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t), 255};
}
}  // namespace
MenuState::MenuState(const Input& input, Renderer& renderer, UiConfig config,
                     const Settings& settings, std::function<void()> start,
                     std::function<bool(const Settings&)> save, std::function<void()> quit,
                     std::string error)
    : input_(input),
      renderer_(renderer),
      config_(config),
      settings_(settings),
      start_(std::move(start)),
      save_(std::move(save)),
      quit_(std::move(quit)),
      error_(std::move(error)) {}
int MenuState::rows() const {
    if (page_ == Page::Settings) return 9;
    if (page_ == Page::Credits) return 1;
#ifdef __EMSCRIPTEN__
    return 3;
#else
    return 4;
#endif
}
WidgetBounds MenuState::bounds(int row) const {
    if (page_ == Page::Settings) return {224.f, 152.f + row * 56.f, 832.f, 48.f};
    if (page_ == Page::Credits) return {96, 568, 352, 56};
    return {96, 328.f + row * 72.f, 352, 56};
}
void MenuState::changePage(Page page) {
    renderer_.startTransition(config_.transitionTime, page == Page::Menu);
    page_ = page;
    focus_ = 0;
    dragged_ = -1;
    buttons_ = {};
    error_.clear();
}
void MenuState::update(float dt) {
    elapsed_ += dt;
    if (input_.backPressed && page_ != Page::Menu) {
        changePage(Page::Menu);
        return;
    }
    const int count = rows();
    focus_ = (focus_ + input_.menuVertical + count) % count;
    const bool moved =
        input_.mouseLogical.x != lastPointer_.x || input_.mouseLogical.y != lastPointer_.y;
    for (int i = 0; i < count; ++i) {
        if (moved && bounds(i).contains(input_.mouseLogical, input_.mouseInViewport)) focus_ = i;
        buttons_[i].update(dt, focus_ == i, config_.hoverTime);
    }
    lastPointer_ = input_.mouseLogical;
    if (!input_.fireHeld) dragged_ = -1;
    if (page_ == Page::Settings) {
        Settings next = settings_;
        float* volumes[] = {&next.volumeMaster, &next.volumeMusic, &next.volumeSfx,
                            &next.volumeVoice};
        for (int i = 0; i < 4; ++i) {
            const auto b = bounds(i);
            if (input_.startClicked && b.contains(input_.mouseLogical, input_.mouseInViewport))
                dragged_ = i;
            if (dragged_ == i && input_.mouseInViewport && (input_.fireHeld || input_.startClicked))
                *volumes[i] = sliderValue(input_.mouseLogical.x, 640, 272, *volumes[i]);
            if (focus_ == i && input_.menuHorizontal)
                *volumes[i] = std::clamp(*volumes[i] + input_.menuHorizontal / 100.f, 0.f, 1.f);
        }
        for (int i = 4; i < count; ++i)
            if (buttons_[i].activated(bounds(i), input_.mouseLogical, input_.mouseInViewport,
                                      input_.startClicked, focus_ == i, input_.confirmPressed) ||
                (focus_ == i && input_.menuHorizontal)) {
                if (i == 4) next.fullscreen = !next.fullscreen;
                if (i == 5) next.hints = !next.hints;
                if (i == 6) next.reduceEffects = !next.reduceEffects;
                if (i == 7) next.difficulty = next.difficulty == "easy" ? "normal" : "easy";
                if (i == 8) {
                    changePage(Page::Menu);
                    return;
                }
            }
        if (SaveStore::encodeSettings(next) != SaveStore::encodeSettings(settings_)) {
            if (save_(next)) {
                error_.clear();
            } else
                error_ = "Settings could not be saved. Please try again.";
        }
        return;
    }
    for (int i = 0; i < count; ++i)
        if (buttons_[i].activated(bounds(i), input_.mouseLogical, input_.mouseInViewport,
                                  input_.startClicked, focus_ == i, input_.confirmPressed)) {
            if (page_ == Page::Credits) {
                changePage(Page::Menu);
                return;
            }
            if (i == 0) {
                start_();
                return;
            }
            if (i == 1)
                changePage(Page::Settings);
            else if (i == 2)
                changePage(Page::Credits);
            else
                quit_();
            return;
        }
}
void MenuState::render(float alpha) {
    (void)alpha;
    const auto& art = renderer_.uiAssets();
    ClearBackground(kInk);
    // Original vector facade: window bays, pilasters, steps and pediment.
    const float drift = settings_.reduceEffects ? 0 : std::sin(elapsed_ * .12f) * 12;
    const float x = 568 + drift;
    DrawRectangleGradientH(480, 0, 800, 720, kInk, {14, 32, 34, 255});
    DrawTriangleLines({x, 216}, {x + 318, 112}, {x + 636, 216}, Fade(kTeal, .55f));
    DrawRectangleLinesEx({x, 224, 636, 344}, 2, Fade(kTeal, .45f));
    for (int i = 0; i < 6; ++i) {
        DrawRectangleLinesEx({x + 24 + i * 104, 252, 24, 288}, 2, Fade(kTeal, .45f));
        if (i < 5)
            for (int j = 0; j < 2; ++j)
                DrawRectangleLinesEx({x + 64 + i * 104, 268.f + j * 112, 48, 80}, 1,
                                     Fade(kBone, .12f));
    }
    for (int i = 0; i < 3; ++i)
        DrawRectangleLinesEx({x - 24.f - i * 16, 568.f + i * 16, 684.f + i * 32, 16}, 1,
                             Fade(kTeal, .35f));
    art.text("GOTHAM CENTRAL BANK", {x + 134, 232}, 16, Fade(kTeal, .65f));
    DrawLine(96, 680, 1184, 680, Fade(kBone, .2f));
    art.text("ARE YOU IN OR OUT?", {96, 692}, 14, kGrey, true);
    art.text("GHOST PROTOCOL", {1000, 692}, 14, kTeal, true);
    if (page_ == Page::Menu) {
        const auto logo = art.logo();
        if (logo.id) {
            const float scale = std::min(392.f / logo.width, 232.f / logo.height);
            DrawTexturePro(logo,
                           {0, 0, static_cast<float>(logo.width), static_cast<float>(logo.height)},
                           {76, 56, logo.width * scale, logo.height * scale}, {0, 0}, 0, WHITE);
        } else
            art.text("GHOST\nPROTOCOL", {96, 96}, 44, kBone, false, true);
        art.text("One bank. One ghost. No witnesses required.", {96, 280}, 18, kGrey, true);
        const char* labels[] = {"START HEIST", "SETTINGS", "CREDITS", "QUIT"};
        for (int i = 0; i < rows(); ++i) {
            const auto b = bounds(i);
            const float t = buttons_[i].emphasis();
            DrawRectangleRounded({b.x, b.y, b.width, b.height}, .28f, 8, mix(kSlate, kBone, t));
            DrawRectangleRoundedLinesEx({b.x, b.y, b.width, b.height}, .28f, 8, 2,
                                        Fade(kBone, .2f));
            art.text(labels[i], {b.x + 24, b.y + 15}, 24, mix(kBone, kInk, t));
        }
        art.text("ARROWS / TAB select   ENTER confirm", {96, 640}, 14, kGrey, true);
    } else if (page_ == Page::Settings) {
        DrawRectangleRounded({192, 64, 896, 624}, .025f, 8, kSlate);
        art.text("SETTINGS", {224, 88}, 28, kBone, false, true);
        const char* labels[] = {"MASTER", "MUSIC",          "SFX",        "VOICE", "FULLSCREEN",
                                "HINTS",  "REDUCE EFFECTS", "DIFFICULTY", "BACK"};
        const float values[] = {settings_.volumeMaster, settings_.volumeMusic, settings_.volumeSfx,
                                settings_.volumeVoice};
        for (int i = 0; i < rows(); ++i) {
            const auto b = bounds(i);
            const float t = buttons_[i].emphasis();
            DrawRectangleRounded({b.x, b.y, b.width, b.height}, .15f, 8, Fade(kBone, t * .08f));
            if (focus_ == i)
                DrawRectangleRoundedLinesEx({b.x, b.y, b.width, b.height}, .15f, 8, 2,
                                            Fade(kTeal, .7f));
            art.text(labels[i], {b.x + 16, b.y + 14}, 18, kBone);
            if (i < 4) {
                DrawRectangle(640, static_cast<int>(b.y + 22), 272, 4, kGrey);
                DrawRectangle(640, static_cast<int>(b.y + 22), static_cast<int>(272 * values[i]), 4,
                              kTeal);
                DrawCircle(static_cast<int>(640 + 272 * values[i]), static_cast<int>(b.y + 24), 8,
                           kBone);
                art.text(TextFormat("%d%%", static_cast<int>(std::round(values[i] * 100))),
                         {960, b.y + 14}, 18, kBone, true);
            } else if (i < 7) {
                const bool on = i == 4   ? settings_.fullscreen
                                : i == 5 ? settings_.hints
                                         : settings_.reduceEffects;
                art.text(on ? "ON" : "OFF", {960, b.y + 14}, 18, on ? kTeal : kGrey);
            } else if (i == 7)
                art.text(settings_.difficulty == "easy" ? "TOURIST / EASY" : "PRO / NORMAL",
                         {744, b.y + 14}, 18, kGold, true);
        }
        art.text("Changes save automatically.  Left / Right adjust.  Esc returns.", {224, 664}, 14,
                 kGrey, true);
    } else {
        DrawRectangleRounded({64, 64, 512, 592}, .03f, 8, kSlate);
        art.text("CREDITS", {96, 96}, 28, kBone, false, true);
        art.text("CREATED BY REDBLUE", {96, 168}, 20, kTeal);
        art.text("Design, code and original logo", {96, 208}, 18, kBone, true);
        art.text("BUILT WITH RAYLIB 5.5", {96, 280}, 20, kTeal);
        art.text("Ramon Santamaria and contributors", {96, 320}, 18, kBone, true);
        art.text("Orbitron - The Orbitron Project Authors", {96, 392}, 18, kBone, true);
        art.text("Inter - Rasmus Andersson and contributors", {96, 424}, 18, kBone, true);
        art.text("Both fonts licensed under SIL Open Font License 1.1", {96, 472}, 14, kGrey, true);
        const auto b = bounds(0);
        const float t = buttons_[0].emphasis();
        DrawRectangleRounded({b.x, b.y, b.width, b.height}, .28f, 8, mix(kSlate, kBone, t));
        DrawRectangleRoundedLinesEx({b.x, b.y, b.width, b.height}, .28f, 8, 2, Fade(kBone, .2f));
        art.text("BACK", {120, 583}, 24, mix(kBone, kInk, t));
    }
    if (!error_.empty()) art.text(error_.c_str(), {96, 24}, 18, {255, 59, 92, 255}, true);
}
