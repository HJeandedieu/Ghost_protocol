#include "states/PlayState.h"

#include <cmath>
#include <utility>

#include "render/Letterbox.h"
#include "render/Renderer.h"

PlayState::PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed)
    : level_(std::move(level)),
      input_(input),
      player_(level_.map.tileCenter(level_.playerSpawn), config.player),
      camera_(player_.pos, config.view),
      seed_(seed) {}

void PlayState::enter() {}
void PlayState::exit() {}
void PlayState::update(float dt) {
    player_.update(dt, input_, level_.map);
    Vec2 cursorOffset{};
    if (input_.mouseInViewport && !debugView_) {
        cursorOffset = {
            camera_.target().x + input_.mouseLogical.x - Letterbox::kWidth * 0.5f - player_.pos.x,
            camera_.target().y + input_.mouseLogical.y - Letterbox::kHeight * 0.5f - player_.pos.y};
        if (cursorOffset.x != 0 || cursorOffset.y != 0)
            facing_ = std::atan2(cursorOffset.y, cursorOffset.x);
    }
    camera_.update(dt, player_.pos, cursorOffset);
#ifndef NDEBUG
    if (input_.debugPressed) debugView_ = !debugView_;
#endif
}
void PlayState::render(float alpha) {
    Renderer::drawLevel(level_, player_, camera_.interpolatedTarget(alpha), facing_, alpha,
                        debugView_, seed_);
}
