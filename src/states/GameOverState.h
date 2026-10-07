#pragma once
#include <array>
#include <functional>
#include <string>

#include "core/Config.h"
#include "states/IState.h"
#include "ui/ScreenNavigation.h"
class Renderer;
class GameOverState : public IState {
   public:
    GameOverState(const Input& input, Renderer& renderer, UiConfig config, std::string quip,
                  std::function<void()> retry, std::function<void()> menu);
    void enter() override {}
    void exit() override {}
    void update(float dt) override;
    void render(float alpha) override;

   private:
    const Input& input_;
    Renderer& renderer_;
    UiConfig config_;
    std::string quip_;
    std::array<std::function<void()>, 2> actions_;
    ScreenNavigation navigation_;
};
