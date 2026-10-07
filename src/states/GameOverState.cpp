#include "states/GameOverState.h"

#include "ui/ScreenDrawing.h"
GameOverState::GameOverState(const Input& input, Renderer& renderer, UiConfig config,
                             std::string quip, std::function<void()> retry,
                             std::function<void()> menu)
    : input_(input),
      renderer_(renderer),
      config_(config),
      quip_(std::move(quip)),
      actions_{{std::move(retry), std::move(menu)}} {}
void GameOverState::update(float dt) {
    if (input_.backPressed) {
        actions_[1]();
        return;
    }
    const int action = navigation_.update(input_, dt, config_.hoverTime, 2, 440);
    if (action >= 0) actions_[action]();
}
void GameOverState::render(float alpha) {
    (void)alpha;
    renderer_.drawFrozenFrame();
    DrawRectangle(0, 0, 1280, 720, Fade(Palette::DarkAlarm, .78f));
    DrawRectangle(72, 136, 1136, 488, {10, 10, 12, 220});
    DrawRectangle(72, 136, 8, 488, Palette::Alarm);
    const auto& art = renderer_.uiAssets();
    art.text("BUSTED", {96, 192}, 44, Palette::Alarm, false, true);
    art.text("HANDLER", {96, 290}, 14, Palette::Teal, true);
    art.text(quip_.c_str(), {96, 320}, 22, Palette::Bone, true);
    drawScreenButton(renderer_, navigation_, 0, 440, "RETRY");
    drawScreenButton(renderer_, navigation_, 1, 440, "MENU");
    art.text("Retry reloads your current stage. Deaths stay on the receipt.", {96, 656}, 18,
             Palette::Bone, true);
}
