#include <gtest/gtest.h>

#include <limits>

#include "render/CharacterPose.h"

TEST(CharacterPose, WalkingArticulatesOppositeLegsAndOnlyTheTrailingKnee) {
    const auto pose = CharacterPose::walking(9.424778f, 48, true);
    EXPECT_NEAR(pose.leftLeg, .35f, .0001f);
    EXPECT_NEAR(pose.rightLeg, -.35f, .0001f);
    EXPECT_FLOAT_EQ(pose.leftKnee, 0);
    EXPECT_NEAR(pose.rightKnee, .42f, .0001f);
}

TEST(CharacterPose, StoppedInvalidAndScaledBodiesKeepSafePresentation) {
    EXPECT_FLOAT_EQ(CharacterPose::walking(10, 48, false).leftLeg, 0);
    EXPECT_FLOAT_EQ(CharacterPose::walking(10, 0, true).leftLeg, 0);
    EXPECT_FLOAT_EQ(
        CharacterPose::walking(std::numeric_limits<float>::infinity(), 48, true).leftLeg, 0);
    EXPECT_FLOAT_EQ(CharacterPose::walking(10, 48, true).leftLeg,
                    CharacterPose::walking(20, 96, true).leftLeg);
}

TEST(WeaponPose, ReloadLowersTheWeaponAndReturnsItAtExistingTimerBoundaries) {
    EXPECT_FLOAT_EQ(WeaponPose::reload(1.4f, 1.4f).lower, 0);
    const auto middle = WeaponPose::reload(.7f, 1.4f);
    EXPECT_NEAR(middle.lower, .22f, .0001f);
    EXPECT_NEAR(middle.turn, -.5f, .0001f);
    EXPECT_FLOAT_EQ(WeaponPose::reload(0, 1.4f).lower, 0);
}

TEST(WeaponPose, InvalidTimersCannotProduceNonfiniteTransforms) {
    EXPECT_FLOAT_EQ(WeaponPose::reload(1, 0).lower, 0);
    EXPECT_FLOAT_EQ(WeaponPose::reload(std::numeric_limits<float>::quiet_NaN(), 1).turn, 0);
    EXPECT_FLOAT_EQ(WeaponPose::reload(2, 1).lower, 0);
    EXPECT_FLOAT_EQ(WeaponPose::reload(-1, 1).lower, 0);
}
