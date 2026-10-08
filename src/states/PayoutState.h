#pragma once
#include <functional>

#include "core/Config.h"
#include "core/Input.h"
#include "states/IState.h"
#include "systems/ScoreSystem.h"
#include "ui/ScreenNavigation.h"
class Renderer;
class AudioDirector;
class PayoutState : public IState {
   public:
    PayoutState(const Input& input, Renderer& renderer, UiConfig config, Payout payout,
                std::function<void()> again, std::function<void()> menu,
                AudioDirector* audio = nullptr);
    void enter() override {}
    void exit() override {}
    void update(float dt) override;
    void render(float alpha) override;
    int visibleLines() const;
    double displayedAmount() const;
    float stampProgress() const;

   private:
    int lineCount() const { return 5 + static_cast<int>(payout_.deductions.size()); }
    const Input& input_;
    Renderer& renderer_;
    UiConfig config_;
    Payout payout_;
    std::function<void()> again_, menu_;
    ScreenNavigation navigation_;
    double elapsed_ = 0;
    AudioDirector* audio_ = nullptr;
};
