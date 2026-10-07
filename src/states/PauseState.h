#pragma once
#include "core/Config.h"
#include "states/IState.h"
#include "ui/ScreenNavigation.h"
class Renderer;
class PauseState : public IState {
   public:
    PauseState(const Input& input, Renderer& renderer, UiConfig config,
               std::array<std::function<void()>, 4> actions);
    void enter() override {}
    void exit() override {}
    void update(float dt) override;
    void render(float alpha) override;

   private:
    const Input& input_;
    Renderer& renderer_;
    UiConfig config_;
    std::array<std::function<void()>, 4> actions_;
    ScreenNavigation navigation_;
};
