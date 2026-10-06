#pragma once

#include <functional>
#include <string>

#include "core/Input.h"
#include "states/IState.h"

class MenuState : public IState {
   public:
    MenuState(const Input& input, std::function<void()> start, std::string error = "");
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;

   private:
    const Input& input_;
    std::function<void()> start_;
    std::string error_;
};
