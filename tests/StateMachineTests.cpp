#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>

#include "states/StateMachine.h"

namespace {
struct Counts {
    int enters = 0;
    int exits = 0;
    int updates = 0;
    int renders = 0;
};
class TestState : public IState {
   public:
    explicit TestState(Counts& counts, std::function<void()> onUpdate = {})
        : counts_(counts), onUpdate_(std::move(onUpdate)) {}
    void enter() override { ++counts_.enters; }
    void exit() override { ++counts_.exits; }
    void update(float dt) override {
        (void)dt;
        ++counts_.updates;
        if (onUpdate_) {
            onUpdate_();
        }
    }
    void render(float alpha) override {
        (void)alpha;
        ++counts_.renders;
    }

   private:
    Counts& counts_;
    std::function<void()> onUpdate_;
};
}  // namespace

TEST(StateMachine, PushFreezesUnderlyingStateAndPopResumesItWithoutReentering) {
    Counts play, pause;
    StateMachine machine;
    machine.push(std::make_unique<TestState>(play));
    machine.push(std::make_unique<TestState>(pause));
    machine.update(0.016f);
    machine.render(0.5f);
    EXPECT_EQ(play.updates, 0);
    EXPECT_EQ(pause.updates, 1);
    EXPECT_EQ(play.renders, 0);
    EXPECT_EQ(pause.renders, 1);
    machine.pop();
    machine.update(0.016f);
    EXPECT_EQ(play.updates, 1);
    EXPECT_EQ(play.enters, 1);
    EXPECT_EQ(pause.exits, 1);
    EXPECT_EQ(machine.size(), 1u);
}

TEST(StateMachine, ReplaceExitsEntireStackAndEntersOnlyTheNewState) {
    Counts play, pause, menu;
    StateMachine machine;
    machine.push(std::make_unique<TestState>(play));
    machine.push(std::make_unique<TestState>(pause));
    machine.replace(std::make_unique<TestState>(menu));
    EXPECT_EQ(play.exits, 1);
    EXPECT_EQ(pause.exits, 1);
    EXPECT_EQ(menu.enters, 1);
    EXPECT_EQ(machine.size(), 1u);
}

TEST(StateMachine, TransitionFromUpdateWaitsUntilCallbackReturns) {
    Counts old, next;
    bool survived = false;
    StateMachine machine;
    machine.push(std::make_unique<TestState>(old, [&] {
        machine.replace(std::make_unique<TestState>(next));
        EXPECT_EQ(old.exits, 0);
        survived = true;
    }));
    machine.update(0.016f);
    EXPECT_TRUE(survived);
    EXPECT_EQ(old.exits, 1);
    EXPECT_EQ(next.enters, 1);
    EXPECT_EQ(next.updates, 0);
}

TEST(StateMachine, EmptyStackIsSafeAndNullStatesAreRejected) {
    StateMachine machine;
    machine.pop();
    machine.update(0.016f);
    machine.render(0.0f);
    EXPECT_EQ(machine.top(), nullptr);
    EXPECT_THROW(machine.push(nullptr), std::invalid_argument);
    EXPECT_THROW(machine.replace(nullptr), std::invalid_argument);
}

TEST(StateMachine, DestructionExitsRemainingStatesOnce) {
    Counts counts;
    {
        StateMachine machine;
        machine.push(std::make_unique<TestState>(counts));
    }
    EXPECT_EQ(counts.enters, 1);
    EXPECT_EQ(counts.exits, 1);
}
