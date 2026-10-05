#include "states/PlayState.h"

#include <utility>

#include "render/Renderer.h"

PlayState::PlayState(Level level, const Input& input, std::uint32_t seed)
    : level_(std::move(level)), input_(input), seed_(seed) {}

void PlayState::enter() {}
void PlayState::exit() {}
void PlayState::update(float dt) {
    (void)dt;
#ifndef NDEBUG
    if (input_.debugPressed) debugView_ = !debugView_;
#endif
}
void PlayState::render(float alpha) {
    (void)alpha;
    Renderer::drawLevel(level_, debugView_, seed_);
}
