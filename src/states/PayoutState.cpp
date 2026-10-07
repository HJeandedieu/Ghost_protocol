#include "states/PayoutState.h"

#include "render/Renderer.h"
void PayoutState::update(float dt) {
    (void)dt;
    if (input_.confirmPressed) menu_();
}
void PayoutState::render(float alpha) {
    (void)alpha;
    Renderer::drawPayout(payout_);
}
