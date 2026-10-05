#pragma once

#include <cstdint>

#include "core/Input.h"
#include "entities/Player.h"
#include "render/FollowCamera.h"
#include "states/IState.h"
#include "world/Level.h"

class PlayState : public IState {
   public:
    PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed);
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;

   private:
    Level level_;
    const Input& input_;
    Player player_;
    FollowCamera camera_;
    float facing_ = 0.0f;
    std::uint32_t seed_;
    bool debugView_ = false;
};
