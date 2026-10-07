#pragma once
#include <functional>

#include "core/Input.h"
#include "states/IState.h"
#include "systems/ScoreSystem.h"
class PayoutState : public IState {
   public:
    PayoutState(const Input& input, Payout payout, std::function<void()> menu)
        : input_(input), payout_(std::move(payout)), menu_(std::move(menu)) {}
    void enter() override {}
    void exit() override {}
    void update(float dt) override;
    void render(float alpha) override;

   private:
    const Input& input_;
    Payout payout_;
    std::function<void()> menu_;
};
