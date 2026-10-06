#include <gtest/gtest.h>

#include "render/Letterbox.h"

TEST(Letterbox, MatchingAspectUsesTheEntireWindow) {
    const auto viewport = Letterbox::fit(1920, 1080);
    EXPECT_FLOAT_EQ(viewport.x, 0.0f);
    EXPECT_FLOAT_EQ(viewport.y, 0.0f);
    EXPECT_FLOAT_EQ(viewport.width, 1920.0f);
    EXPECT_FLOAT_EQ(viewport.height, 1080.0f);
}
TEST(Letterbox, WideWindowCentersWithSideBars) {
    const auto viewport = Letterbox::fit(1600, 720);
    EXPECT_FLOAT_EQ(viewport.x, 160.0f);
    EXPECT_FLOAT_EQ(viewport.y, 0.0f);
    EXPECT_FLOAT_EQ(viewport.width, 1280.0f);
    EXPECT_FLOAT_EQ(viewport.height, 720.0f);
}
TEST(Letterbox, TallWindowCentersWithTopAndBottomBars) {
    const auto viewport = Letterbox::fit(1280, 1024);
    EXPECT_FLOAT_EQ(viewport.x, 0.0f);
    EXPECT_FLOAT_EQ(viewport.y, 152.0f);
    EXPECT_FLOAT_EQ(viewport.width / viewport.height, 1280.0f / 720.0f);
}
TEST(Letterbox, MinimizedWindowHasNoDrawableViewport) {
    EXPECT_FLOAT_EQ(Letterbox::fit(0, 0).width, 0.0f);
    EXPECT_FLOAT_EQ(Letterbox::fit(-1, 720).height, 0.0f);
}
