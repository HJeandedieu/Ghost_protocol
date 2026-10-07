#include <gtest/gtest.h>

#include "ui/Widgets.h"

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
