#pragma once

#include <memory>
#include <vector>

#include "states/IState.h"

class StateMachine {
   public:
    ~StateMachine();
    void push(std::unique_ptr<IState> state);
    void pop();
    void replace(std::unique_ptr<IState> state);
    void update(float dt);
    void render(float alpha);
    IState* top();
    const IState* top() const;
    std::size_t size() const;

   private:
    enum class Action { Push, Pop, Replace };
    struct Transition {
        Action action;
        std::unique_ptr<IState> state;
    };
    void request(Transition transition);
    void apply(Transition transition);
    void flush();
    std::vector<std::unique_ptr<IState>> stack_;
    std::vector<Transition> pending_;
    bool inCallback_ = false;
};
