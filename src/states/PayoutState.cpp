#include "states/PayoutState.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ui/ScreenDrawing.h"
PayoutState::PayoutState(const Input& input, Renderer& renderer, UiConfig config, Payout payout,
                         std::function<void()> again, std::function<void()> menu)
    : input_(input),
      renderer_(renderer),
      config_(config),
      payout_(std::move(payout)),
      again_(std::move(again)),
      menu_(std::move(menu)) {}
void PayoutState::update(float dt) {
    if (std::isfinite(dt) && dt > 0) elapsed_ += dt;
    const int action = navigation_.update(input_, dt, config_.hoverTime, 2, 440);
    if (input_.backPressed || action == 1) {
        menu_();
        return;
    }
    if (action == 0) again_();
}
int PayoutState::visibleLines() const {
    if (renderer_.reduceEffects()) return lineCount();
    return static_cast<int>(
        std::min(static_cast<double>(lineCount()), elapsed_ / config_.payoutLineTime));
}
double PayoutState::displayedAmount() const {
    if (renderer_.reduceEffects()) return payout_.finalAmount;
    const double t =
        std::clamp((elapsed_ - static_cast<double>(lineCount()) * config_.payoutLineTime) /
                       config_.payoutCountTime,
                   0.0, 1.0);
    const double remainder = 1 - t;
    return payout_.finalAmount * (1 - remainder * remainder * remainder);
}
float PayoutState::stampProgress() const {
    const double start =
        renderer_.reduceEffects()
            ? 0
            : static_cast<double>(lineCount()) * config_.payoutLineTime + config_.payoutCountTime;
    return static_cast<float>(std::clamp((elapsed_ - start) / config_.payoutStampTime, 0.0, 1.0));
}
void PayoutState::render(float alpha) {
    (void)alpha;
    ClearBackground({10, 10, 12, 255});
    const auto& art = renderer_.uiAssets();
    art.text("JOB COMPLETE", {96, 80}, 20, Palette::Teal, false, true);
    art.text("YOUR CUT", {96, 144}, 44, Palette::Bone, false, true);
    art.text("GOTHAM CENTRAL BANK", {96, 216}, 18, Palette::Bone, true);
    art.text("One night. One very expensive resignation.", {96, 248}, 18, Palette::Teal, true);
    drawScreenButton(renderer_, navigation_, 0, 440, "PLAY AGAIN");
    drawScreenButton(renderer_, navigation_, 1, 440, "MENU");
    const Rectangle paper{552, 48, 632, 544};
    DrawRectangleRec({paper.x + 12, paper.y + 12, paper.width, paper.height}, Palette::DeepTeal);
    DrawRectangleRec(paper, Palette::Bone);
    art.text("TRANSFER RECEIPT", {584, 72}, 24, {10, 10, 12, 255}, false, true);
    art.text("HANDLER / NIGHT DISPATCH", {584, 106}, 14, Palette::Teal, true);
    DrawLine(584, 136, 1152, 136, Palette::Teal);
    const auto line = [&](int index, const char* label, double value) {
        if (index >= visibleLines()) return;
        const float y = 156 + static_cast<float>(index) * 32;
        art.text(label, {584, y}, 18, {10, 10, 12, 255}, true);
        const auto amount =
            TextFormat("%s$%.0f", value < 0 ? "-" : "", std::round(std::abs(value)));
        const float width = MeasureTextEx(art.body(), amount, 18, 1).x;
        art.text(amount, {1152 - width, y}, 18, {10, 10, 12, 255}, true);
    };
    line(0, "Delivered cash", payout_.subtotal);
    line(1, "Ghost bonus", payout_.ghostBonus);
    line(2, "Time bonus", payout_.timeBonus);
    line(3, "Handler's cut", -payout_.handlerCut);
    const char* deductions[] = {"Getaway van parking", "Dry cleaning (pink dye)",
                                "Bribe for the raccoon"};
    for (std::size_t i = 0; i < payout_.deductions.size(); ++i)
        line(4 + static_cast<int>(i), deductions[i % 3], -payout_.deductions[i]);
    line(lineCount() - 1, "Death penalty", -payout_.deathPenalty);
    DrawLine(584, 426, 1152, 426, Palette::Teal);
    art.text("FINAL PAYOUT", {584, 450}, 20, Palette::Teal, false, true);
    art.text(TextFormat("$%.0f", std::round(displayedAmount())), {584, 490}, 36, {10, 10, 12, 255},
             false, true);
    const float progress = stampProgress();
    if (progress > 0) {
        const float size = renderer_.reduceEffects() ? 72 : 72 + (1 - progress) * 40;
        const Color ink = Fade(Palette::DeepTeal, progress);
        DrawRectangleLinesEx({1016, 450, 128, 108}, 3, ink);
        art.text(TextFormat("%c", payout_.rank), {1044, 468}, size, ink, false, true);
    }
    art.text("HANDLER", {96, 622}, 14, Palette::Teal, false, true);
    art.text("Fun fact: I'm the bank's night dispatcher. Eleven years, same cameras, same salary.",
             {96, 646}, 18, Palette::Bone, true);
    art.text("Consider this my resignation. I quit.", {96, 672}, 18, Palette::Bone, true);
}
