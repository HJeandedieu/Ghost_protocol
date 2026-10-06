#include "states/BootState.h"

#include <utility>

#include "render/Renderer.h"

BootState::BootState(const Input& input, bool waitForClick, std::function<void()> next)
    : input_(input), waitForClick_(waitForClick), next_(std::move(next)) {}
void BootState::enter() {}
void BootState::exit() {}
void BootState::update(float dt) {
    (void)dt;
    if (!waitForClick_ || input_.startClicked) {
        next_();
    }
}
void BootState::render(float alpha) {
    (void)alpha;
    Renderer::drawPlaceholder("GHOST PROTOCOL", waitForClick_ ? "Click to start" : "Starting...");
}
