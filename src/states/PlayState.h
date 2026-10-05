#pragma once

#include <cstdint>

#include "core/Input.h"
#include "states/IState.h"
#include "world/Level.h"

class PlayState : public IState {
   public:
    PlayState(Level level, const Input& input, std::uint32_t seed);
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;

   private:
    Level level_;
    const Input& input_;
    std::uint32_t seed_;
    bool debugView_ = false;
};
