#include "states/StateMachine.h"

#include <stdexcept>
#include <utility>

StateMachine::~StateMachine() {
    while (!stack_.empty()) {
        stack_.back()->exit();
        stack_.pop_back();
    }
}

void StateMachine::push(std::unique_ptr<IState> state) {
    if (!state) {
        throw std::invalid_argument("Cannot push a null state");
    }
    request({Action::Push, std::move(state)});
}

void StateMachine::pop() { request({Action::Pop, nullptr}); }

void StateMachine::replace(std::unique_ptr<IState> state) {
    if (!state) {
        throw std::invalid_argument("Cannot replace with a null state");
    }
    request({Action::Replace, std::move(state)});
}

void StateMachine::request(Transition transition) {
    if (inCallback_) {
        pending_.push_back(std::move(transition));
    } else {
        apply(std::move(transition));
        flush();
    }
}

void StateMachine::apply(Transition transition) {
    inCallback_ = true;
    if (transition.action == Action::Replace) {
        while (!stack_.empty()) {
            stack_.back()->exit();
            stack_.pop_back();
        }
    } else if (transition.action == Action::Pop && !stack_.empty()) {
        stack_.back()->exit();
        stack_.pop_back();
    }
    if (transition.state) {
        stack_.push_back(std::move(transition.state));
        stack_.back()->enter();
    }
    inCallback_ = false;
}

void StateMachine::flush() {
    // Apply only this batch. Transitions requested from enter/exit wait for the
    // next safe boundary, avoiding an unbounded chain of lifecycle callbacks.
    auto transitions = std::move(pending_);
    pending_.clear();
    for (auto& transition : transitions) {
        apply(std::move(transition));
    }
}

void StateMachine::update(float dt) {
    flush();
    if (!stack_.empty()) {
        inCallback_ = true;
        stack_.back()->update(dt);
        inCallback_ = false;
    }
    flush();
}

void StateMachine::render(float alpha) {
    if (!stack_.empty()) {
        inCallback_ = true;
        stack_.back()->render(alpha);
        inCallback_ = false;
    }
    flush();
}

const IState* StateMachine::top() const { return stack_.empty() ? nullptr : stack_.back().get(); }
std::size_t StateMachine::size() const { return stack_.size(); }
