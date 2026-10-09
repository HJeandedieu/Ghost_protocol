#include <gtest/gtest.h>

#include <limits>

#include "core/FirstPersonView.h"
#include "core/Input.h"

TEST(FirstPersonView, MovementFollowsYawAndNormalizesDiagonals) {
    FirstPersonView view({}, 90);
    const auto forward = view.movement({0, -1});
    EXPECT_NEAR(forward.x, 0, 1e-6);
    EXPECT_NEAR(forward.y, 1, 1e-6);
    const auto right = view.movement({1, 0});
    EXPECT_NEAR(right.x, -1, 1e-6);
    EXPECT_NEAR(right.y, 0, 1e-6);
    const auto diagonal = view.movement({1, -1});
    EXPECT_NEAR(diagonal.x, -0.70710678f, 1e-6);
    EXPECT_NEAR(diagonal.y, 0.70710678f, 1e-6);
    const auto back = view.movement({0, 1});
    EXPECT_NEAR(back.y, -1, 1e-6);
}

TEST(FirstPersonView, LookWrapsYawClampsPitchAndSelectsCrouchHeight) {
    ViewConfig config;
    config.mouseDegPerPx = .5f;
    config.pitchLimitDeg = 60;
    FirstPersonView view(config, 170);
    view.look({40, 1000});
    EXPECT_FLOAT_EQ(view.yawDeg(), -170);
    EXPECT_FLOAT_EQ(view.pitchDeg(), 60);
    view.look({0, -1000});
    EXPECT_FLOAT_EQ(view.pitchDeg(), -60);
    EXPECT_FLOAT_EQ(view.eyeHeight(false), 36);
    EXPECT_FLOAT_EQ(view.eyeHeight(true), 22);
    FirstPersonView retry(config);
    EXPECT_FLOAT_EQ(retry.yawDeg(), 0);
    EXPECT_FLOAT_EQ(retry.pitchDeg(), 0);
}

TEST(FirstPersonView, RetainedFrameDeltaIsConsumedOnlyByTheFirstCatchupTick) {
    Input input;
    input.mouseDelta = {20, 10};
    // A frame with no fixed step keeps its deltas; the next rendered frame adds to them.
    input.mouseDelta.x += 30;
    FirstPersonView view({});
    for (int tick = 0; tick < 4; ++tick) {
        view.look(input.mouseDelta);
        input.clearEdges();
    }
    EXPECT_FLOAT_EQ(view.yawDeg(), 5);
    EXPECT_FLOAT_EQ(view.pitchDeg(), 1);
}

TEST(FirstPersonView, InvalidDeltasAndMovementDoNotCorruptThePose) {
    FirstPersonView view({}, 45);
    view.look({std::numeric_limits<float>::infinity(), 100});
    EXPECT_FLOAT_EQ(view.yawDeg(), 45);
    EXPECT_FLOAT_EQ(view.pitchDeg(), 0);
    const auto move = view.movement({0, std::numeric_limits<float>::quiet_NaN()});
    EXPECT_FLOAT_EQ(move.x, 0);
    EXPECT_FLOAT_EQ(move.y, 0);
    view.look({std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});
    EXPECT_LE(view.yawDeg(), 180);
    EXPECT_GE(view.yawDeg(), -180);
    EXPECT_FLOAT_EQ(view.pitchDeg(), 80);
}

TEST(FirstPersonView, ShotRayUsesUnshakenYawPitchAndCurrentEyeHeight) {
    FirstPersonView view({}, 90);
    view.look({0, 300});
    const auto standing = view.shotRay({120, 240}, false);
    const auto crouched = view.shotRay({120, 240}, true);
    EXPECT_FLOAT_EQ(standing.origin.x, 120);
    EXPECT_FLOAT_EQ(standing.origin.z, 240);
    EXPECT_FLOAT_EQ(standing.origin.y, 36);
    EXPECT_FLOAT_EQ(crouched.origin.y, 22);
    EXPECT_NEAR(standing.direction.y, -.5f, .000001);
    EXPECT_NEAR(standing.direction.z, std::sqrt(.75f), .000001);
    EXPECT_FLOAT_EQ(standing.direction.y, crouched.direction.y);
    EXPECT_NEAR(standing.direction.dot(standing.direction), 1, .000001);
}
