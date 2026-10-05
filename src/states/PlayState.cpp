#include "states/PlayState.h"

#include "render/Renderer.h"

void PlayState::enter() {}
void PlayState::exit() {}
void PlayState::update(float dt) { (void)dt; }
void PlayState::render(float alpha) {
    (void)alpha;
    Renderer::drawPlaceholder("GOTHAM CENTRAL BANK", "Play placeholder - level rendering is next");
}
