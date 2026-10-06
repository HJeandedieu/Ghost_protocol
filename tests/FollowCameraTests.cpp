#include <gtest/gtest.h>

#include <cmath>

#include "render/FollowCamera.h"

TEST(FollowCamera, FollowsWithoutOvershootingAndInterpolatesPreviousTarget) {
    ViewConfig config;
    FollowCamera camera({0, 0}, config);
    camera.update(1.0f / 60.0f, {100, 0}, {});
    EXPECT_GT(camera.target().x, 0);
    EXPECT_LT(camera.target().x, 100);
    EXPECT_FLOAT_EQ(camera.interpolatedTarget(0).x, 0);
    EXPECT_FLOAT_EQ(camera.interpolatedTarget(1).x, camera.target().x);
    EXPECT_NEAR(camera.interpolatedTarget(0.5f).x, camera.target().x * 0.5f, 0.001f);
}

TEST(FollowCamera, MouseLeadIsBoundedAndSmoothNearThePlayer) {
    FollowCamera far({100, 100}, ViewConfig{});
    far.update(10, {100, 100}, {300, 400});
    EXPECT_NEAR(far.target().x, 136, 0.001f);
    EXPECT_NEAR(far.target().y, 148, 0.001f);
    FollowCamera near({100, 100}, ViewConfig{});
    near.update(10, {100, 100}, {3, 4});
    EXPECT_NEAR(near.target().x, 103, 0.001f);
    EXPECT_NEAR(near.target().y, 104, 0.001f);
}

TEST(FollowCamera, ResponseIsIndependentOfTickSubdivisionAndZeroSettingsAreSafe) {
    FollowCamera whole({}, ViewConfig{}), split({}, ViewConfig{});
    whole.update(0.2f, {100, 20}, {});
    for (int i = 0; i < 12; ++i) split.update(1.0f / 60.0f, {100, 20}, {});
    EXPECT_NEAR(whole.target().x, split.target().x, 0.001f);
    ViewConfig config;
    config.leadPx = 0;
    FollowCamera noLead({}, config);
    noLead.update(10, {100, 20}, {300, 400});
    EXPECT_NEAR(noLead.target().x, 100, 0.001f);
    config.followRate = 0;
    FollowCamera frozen({5, 6}, config);
    frozen.update(1, {100, 20}, {});
    EXPECT_FLOAT_EQ(frozen.target().x, 5);
    frozen.update(-1, {100, 20}, {});
    EXPECT_FLOAT_EQ(frozen.target().y, 6);
}
