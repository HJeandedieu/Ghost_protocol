#include <gtest/gtest.h>

#include "ui/ScreenNavigation.h"

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
#include "ui/Widgets.h"

TEST(ScreenNavigation, KeyboardWrapsAndClearedEdgesDoNotRepeatActions) {
    ScreenNavigation navigation;
    Input input;
    input.menuVertical = -1;
    input.confirmPressed = true;
    EXPECT_EQ(navigation.update(input, .016f, .15f, 4, 328), 3);
    input.clearEdges();
    EXPECT_EQ(navigation.update(input, .016f, .15f, 4, 328), -1);
    input.menuVertical = 1;
    input.confirmPressed = true;
    EXPECT_EQ(navigation.update(input, .016f, .15f, 4, 328), 0);
}
TEST(ScreenNavigation, MouseActivationRejectsLetterboxAndOutsideButton) {
    ScreenNavigation navigation;
    Input input;
    input.startClicked = true;
    input.mouseLogical = {120, 520};
    input.mouseInViewport = false;
    EXPECT_EQ(navigation.update(input, .016f, .15f, 2, 440), -1);
    input.mouseInViewport = true;
    EXPECT_EQ(navigation.update(input, .016f, .15f, 2, 440), 1);
    input.mouseLogical.x = 448;
    EXPECT_EQ(navigation.update(input, .016f, .15f, 2, 440), -1);
}
TEST(Widgets, HoverUsesCubicEaseAndReturnsToRest) {
    Button button;
    button.update(.075f, true, .15f);
    EXPECT_NEAR(button.emphasis(), .875f, .0001f);
    button.update(1, true, .15f);
    EXPECT_FLOAT_EQ(button.emphasis(), 1);
    button.update(1, false, .15f);
    EXPECT_FLOAT_EQ(button.emphasis(), 0);
}
TEST(Widgets, ActivationRequiresViewportOrKeyboardFocusAndIgnoresInvalidTiming) {
    Button button;
    WidgetBounds bounds{10, 10, 100, 56};
    EXPECT_TRUE(button.activated(bounds, {20, 20}, true, true, false, false));
    EXPECT_FALSE(button.activated(bounds, {20, 20}, false, true, false, false));
    EXPECT_FALSE(button.activated(bounds, {110, 20}, true, true, false, false));
    EXPECT_TRUE(button.activated(bounds, {}, false, false, true, true));
    EXPECT_FALSE(button.activated(bounds, {}, false, false, false, true));
    button.update(-1, true, .15f);
    button.update(1, true, 0);
    EXPECT_FLOAT_EQ(button.emphasis(), 0);
}
TEST(Widgets, SliderClampsAndInvalidTrackPreservesPreviousValue) {
    EXPECT_FLOAT_EQ(sliderValue(75, 50, 100, 0), .25f);
    EXPECT_FLOAT_EQ(sliderValue(-100, 50, 100, .5f), 0);
    EXPECT_FLOAT_EQ(sliderValue(500, 50, 100, .5f), 1);
    EXPECT_FLOAT_EQ(sliderValue(75, 50, 0, .5f), .5f);
    EXPECT_FLOAT_EQ(transitionProgress(.125f, .25f), .875f);
    EXPECT_FLOAT_EQ(transitionProgress(1, .25f), 1);
}
