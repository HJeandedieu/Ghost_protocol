#include <gtest/gtest.h>

#include <limits>

#include "core/Time.h"

TEST(Time, AccumulatesShortFramesAndInterpolatesRemainder) {
    Time time;
    time.addFrame(Time::kStep * 0.5);
    EXPECT_FALSE(time.consumeStep());
    EXPECT_FLOAT_EQ(time.alpha(), 0.5f);
    time.addFrame(Time::kStep);
    EXPECT_TRUE(time.consumeStep());
    EXPECT_FALSE(time.consumeStep());
    EXPECT_NEAR(time.alpha(), 0.5f, 0.00001f);
}

TEST(Time, AdvancesExactlySixtyTicksForOneSecond) {
    Time time;
    int ticks = 0;
    for (int frame = 0; frame < 120; ++frame) {
        time.addFrame(1.0 / 120.0);
        while (time.consumeStep()) {
            ++ticks;
        }
    }
    EXPECT_EQ(ticks, 60);
    EXPECT_NEAR(time.alpha(), 0.0f, 0.00001f);
}

TEST(Time, ClampsLongStallsInsteadOfCreatingUnlimitedCatchUp) {
    Time time;
    time.addFrame(30.0);
    int ticks = 0;
    while (time.consumeStep()) {
        ++ticks;
    }
    EXPECT_EQ(ticks, 15);
    EXPECT_NEAR(time.alpha(), 0.0f, 0.00001f);
}

TEST(Time, InvalidFrameTimesAreIgnoredAndResetClearsRemainder) {
    Time time;
    time.addFrame(-1.0);
    time.addFrame(std::numeric_limits<double>::quiet_NaN());
    time.addFrame(std::numeric_limits<double>::infinity());
    EXPECT_FALSE(time.consumeStep());
    EXPECT_FLOAT_EQ(time.alpha(), 0.0f);
    time.addFrame(Time::kStep * 0.5);
    time.reset();
    EXPECT_FLOAT_EQ(time.alpha(), 0.0f);
}
