#include "states/LoadoutState.h"

#include "render/Renderer.h"
void LoadoutState::update(float dt) {
    (void)dt;
    if (input_.backPressed && back_) {
        back_();
        return;
    }
    if (input_.loadoutExcluded >= 0 && input_.loadoutExcluded < 3)
        excluded_ = input_.loadoutExcluded;
    if (input_.crouchPressed) easy_ = !easy_;
    if (input_.confirmPressed) {
        const std::array<std::string, 3> weapons{{"whisper", "chatter", "gavel"}};
        std::array<std::string, 2> selected;
        int slot = 0;
        for (int i = 0; i < 3; ++i)
            if (i != excluded_) selected[slot++] = weapons[i];
        start_(std::move(selected), easy_);
    }
}
void LoadoutState::render(float alpha) {
    (void)alpha;
    Renderer::drawLoadout(excluded_, easy_);
}
