#pragma once

#include <functional>

#include "core/Input.h"
#include "states/IState.h"

class MenuState : public IState {
   public:
    MenuState(const Input& input, std::function<void()> start);
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;

   private:
    const Input& input_;
    std::function<void()> start_;
};
