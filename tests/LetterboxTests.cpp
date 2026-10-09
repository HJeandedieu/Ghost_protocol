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

TEST(Letterbox, SupersamplingCoversPhysicalOutputWithExactAspect) {
    const auto full = Letterbox::renderSize(1920, 1080, 2, 8192);
    EXPECT_EQ(full.width, 3840);
    EXPECT_EQ(full.height, 2160);
    EXPECT_FALSE(full.reduced);
    EXPECT_EQ(Letterbox::renderSize(1280, 1024, 2, 8192).width, 2560);
    EXPECT_EQ(Letterbox::renderSize(0, 0, 2, 8192).height, 1440);
}
TEST(Letterbox, TextureLimitPreservesAspectInsteadOfStretchingOrOverflowing) {
    const auto limited = Letterbox::renderSize(1920, 1080, 4, 2048);
    EXPECT_EQ(limited.width, 2048);
    EXPECT_EQ(limited.height, 1152);
    EXPECT_TRUE(limited.reduced);
    EXPECT_EQ(Letterbox::renderSize(2147483647, 2147483647, 4, 8192).width, 8192);
    EXPECT_EQ(Letterbox::renderSize(1280, 720, 2, 0).width, 0);
}
