#pragma once

#include "states/IState.h"

class PlayState : public IState {
   public:
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;
};
