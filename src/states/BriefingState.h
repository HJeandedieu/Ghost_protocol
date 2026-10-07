#pragma once
#include "core/Config.h"
#include "states/IState.h"
#include "ui/ScreenNavigation.h"
class Renderer;
class BriefingState : public IState {
   public:
    BriefingState(const Input& input, Renderer& renderer, UiConfig config,
                  std::function<void()> start, std::function<void()> back);
    void enter() override {}
    void exit() override {}
    void update(float dt) override;
    void render(float alpha) override;
    int slide() const { return slide_; }

   private:
    const Input& input_;
    Renderer& renderer_;
    UiConfig config_;
    std::function<void()> start_, back_;
    ScreenNavigation navigation_;
    int slide_ = 0;
    float elapsed_ = 0;
};
