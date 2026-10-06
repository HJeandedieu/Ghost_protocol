#include "states/MenuState.h"

#include <utility>

#include "render/Renderer.h"

MenuState::MenuState(const Input& input, std::function<void()> start, std::string error)
    : input_(input), start_(std::move(start)), error_(std::move(error)) {}
void MenuState::enter() {}
void MenuState::exit() {}
void MenuState::update(float dt) {
    (void)dt;
    if (input_.confirmPressed) {
        start_();
    }
}
void MenuState::render(float alpha) {
    (void)alpha;
    Renderer::drawPlaceholder("GHOST PROTOCOL", "Press ENTER to start");
    if (!error_.empty()) Renderer::drawError(error_.c_str());
}
