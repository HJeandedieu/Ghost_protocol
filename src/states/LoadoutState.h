#pragma once
#include <array>
#include <functional>
#include <string>

#include "core/Input.h"
#include "states/IState.h"
class LoadoutState : public IState {
   public:
    LoadoutState(const Input& input, std::function<void(std::array<std::string, 2>, bool)> start,
                 bool easy = false, std::function<void()> back = {})
        : input_(input), start_(std::move(start)), back_(std::move(back)), easy_(easy) {}
    void enter() override {}
    void exit() override {}
    void update(float dt) override;
    void render(float alpha) override;

   private:
    const Input& input_;
    std::function<void(std::array<std::string, 2>, bool)> start_;
    std::function<void()> back_;
    int excluded_ = 2;
    bool easy_ = false;
};
