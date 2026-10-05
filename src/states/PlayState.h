#pragma once

#include <cstdint>

#include "core/EventBus.h"
#include "core/Input.h"
#include "entities/Player.h"
#include "render/FollowCamera.h"
#include "states/IState.h"
#include "systems/InteractionSystem.h"
#include "systems/NoiseSystem.h"
#include "systems/RippleSystem.h"
#include "world/Level.h"
#include "world/World.h"

class Renderer;

class PlayState : public IState {
   public:
    PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed,
              Renderer& renderer);
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;

   private:
    EventBus events_;
    World world_;
    const Input& input_;
    FollowCamera camera_;
    float facing_ = 0.0f;
    std::uint32_t seed_;
    bool debugView_ = false;
    RippleSystem ripple_;
    std::vector<Entity*> revealables_;
    Renderer& renderer_;
    NoiseSystem noise_;
    InteractionSystem interaction_;
    const Config& config_;
};
