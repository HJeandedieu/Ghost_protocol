#include <gtest/gtest.h>

#include "core/GameplayInputGate.h"

TEST(GameplayInputGate, HeldRestartClickCannotFireUntilReleased) {
    GameplayInputGate gate;
    Input input;
    input.fireHeld = input.firePressed = input.startClicked = true;
    auto gameplay = gate.filter(input);
    EXPECT_FALSE(gameplay.fireHeld);
    EXPECT_FALSE(gameplay.firePressed);
    EXPECT_TRUE(input.fireHeld);
    EXPECT_TRUE(gameplay.startClicked);
    input.clearEdges();
    EXPECT_FALSE(gate.filter(input).fireHeld);
    input.fireHeld = false;
    gate.filter(input);
    input.fireHeld = input.firePressed = true;
    EXPECT_TRUE(gate.filter(input).fireHeld);
    EXPECT_TRUE(gate.filter(input).firePressed);
    gate.blockFire();
    EXPECT_FALSE(gate.filter(input).fireHeld);
}
TEST(GameplayInputGate, KeyboardResumeAllowsTheNextFreshMousePress) {
    GameplayInputGate gate;
    Input input;
    input.confirmPressed = true;
    EXPECT_TRUE(gate.filter(input).confirmPressed);
    input.clearEdges();
    input.fireHeld = input.firePressed = true;
    EXPECT_TRUE(gate.filter(input).firePressed);
}
