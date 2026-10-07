#include <gtest/gtest.h>

#include "ui/ScreenNavigation.h"

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
