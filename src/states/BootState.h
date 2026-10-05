#pragma once

#include <functional>

#include "core/Input.h"
#include "states/IState.h"

class BootState : public IState {
   public:
    BootState(const Input& input, bool waitForClick, std::function<void()> next);
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;

   private:
    const Input& input_;
    bool waitForClick_;
    std::function<void()> next_;
};
