#include <gtest/gtest.h>

#include <limits>

#include "core/ViewFootprint.h"
TEST(ViewFootprint, IncludesForwardVolumeAndRejectsBehindAndBeyondFarPlane) {
    const auto view = ViewFootprint::perspective(ShotRay::aim({0, 0}, 36, 0), 90, 1, 1, 100);
    EXPECT_GE(view.count, 3);
    EXPECT_TRUE(view.intersects({50, 0}, 1));
    EXPECT_FALSE(view.intersects({-10, 0}, 1));
    EXPECT_FALSE(view.intersects({110, 0}, 1));
    EXPECT_TRUE(view.intersects({100, 0}, 1));
}
TEST(ViewFootprint, AspectPitchAndRotationAffectConservativeProjection) {
    const auto narrow = ViewFootprint::perspective(ShotRay::aim({0, 0}, 36, 0), 60, 1, 1, 100);
    const auto wide = ViewFootprint::perspective(ShotRay::aim({0, 0}, 36, 0), 60, 2, 1, 100);
    EXPECT_FALSE(narrow.intersects({50, 45}, 1));
    EXPECT_TRUE(wide.intersects({50, 45}, 1));
    const auto rotated =
        ViewFootprint::perspective(ShotRay::aim({0, 0}, 22, 180, 60), 70, 2, 1, 100);
    EXPECT_TRUE(rotated.intersects({-30, 0}, 1));
    EXPECT_FALSE(rotated.intersects({100, 0}, 1));
}
TEST(ViewFootprint, ExactCircleTangencyAndInvalidViewsWaitConservatively) {
    ViewFootprint square{{Vec2{0, 0}, Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10}}, 4};
    EXPECT_TRUE(square.intersects({15, 5}, 5));
    EXPECT_FALSE(square.intersects({15.1f, 5}, 5));
    EXPECT_TRUE(ViewFootprint::perspective({}, 70, 1, 1, 100).intersects({1000, 1000}, 1));
    EXPECT_TRUE(square.intersects({0, 0}, std::numeric_limits<float>::quiet_NaN()));
}
