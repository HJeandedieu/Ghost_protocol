#include "states/PauseState.h"

#include "ui/ScreenDrawing.h"
PauseState::PauseState(const Input& input, Renderer& renderer, UiConfig config,
                       std::array<std::function<void()>, 4> actions)
    : input_(input), renderer_(renderer), config_(config), actions_(std::move(actions)) {}
void PauseState::update(float dt) {
    if (input_.backPressed) {
        actions_[0]();
        return;
    }
    const int action = navigation_.update(input_, dt, config_.hoverTime, 4, 328);
    if (action >= 0) actions_[action]();
}
void PauseState::render(float alpha) {
    (void)alpha;
    renderer_.drawFrozenFrame();
    DrawRectangle(0, 0, 1280, 720, {10, 10, 12, 190});
    const auto& art = renderer_.uiAssets();
    const auto wordmark = art.wordmark();
    if (wordmark.id) {
        const float scale = std::min(360.f / wordmark.width, 88.f / wordmark.height);
        DrawTexturePro(
            wordmark,
            {0, 0, static_cast<float>(wordmark.width), static_cast<float>(wordmark.height)},
            {824, 64, wordmark.width * scale, wordmark.height * scale}, {0, 0}, 0, WHITE);
    }
    art.text("PAUSED", {96, 160}, 44, Palette::Bone, false, true);
    art.text("Take a breath. The bank can wait.", {96, 236}, 18, Palette::Teal, true);
    const char* labels[] = {"RESUME", "SETTINGS", "RESTART STAGE", "QUIT TO MENU"};
    for (int i = 0; i < 4; ++i) drawScreenButton(renderer_, navigation_, i, 328, labels[i]);
    art.text("ESC resume   ARROWS / TAB select   ENTER confirm", {96, 652}, 14, Palette::Bone,
             true);
}
