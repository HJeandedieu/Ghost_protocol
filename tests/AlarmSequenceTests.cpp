#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/EventBus.h"
#include "core/Logger.h"
#include "render/AlarmSequence.h"
TEST(AlarmSequence, QuietIsNeutralAndAlarmStartsOnce) {
    EventBus bus;
    AlarmSequence sequence(bus, AlarmConfig{}, 42);
    EXPECT_FLOAT_EQ(sequence.advance(10), 10);
    EXPECT_EQ(sequence.elapsed(), 0);
    EXPECT_FALSE(sequence.bannerVisible());
    EXPECT_FLOAT_EQ(sequence.paletteBlend(), 0);
    EXPECT_FLOAT_EQ(sequence.barsFraction(), 0);
    bus.publish(AlarmTriggered{AlarmReason::Shot});
    bus.dispatch();
    EXPECT_TRUE(sequence.started());
    EXPECT_TRUE(sequence.bannerVisible());
    sequence.advance(0.4f);
    const auto elapsed = sequence.elapsed();
    bus.publish(AlarmTriggered{AlarmReason::Pager});
    bus.dispatch();
    EXPECT_EQ(sequence.elapsed(), elapsed);
    EXPECT_NEAR(sequence.paletteBlend(), 1, 0.00001);
}
TEST(AlarmSequence, SlowMotionUsesRealTimeAndSplitsTheBoundary) {
    EventBus bus;
    AlarmSequence sequence(bus, AlarmConfig{}, 42);
    bus.publish(AlarmTriggered{AlarmReason::Shot});
    bus.dispatch();
    EXPECT_NEAR(sequence.advance(0.7f), 0.245, 0.00001);
    EXPECT_NEAR(sequence.advance(0.2f), 0.135, 0.00001);
    EXPECT_NEAR(sequence.elapsed(), 0.9, 0.00001);
    EXPECT_FLOAT_EQ(sequence.advance(1), 1);
    EXPECT_FLOAT_EQ(sequence.advance(-1), 0);
    EXPECT_FLOAT_EQ(sequence.advance(std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_NEAR(sequence.elapsed(), 1.9, 0.00001);
}
TEST(AlarmSequence, BarsFlipBannerAndShakeFinishAtDocumentedTimes) {
    EventBus bus;
    AlarmSequence sequence(bus, AlarmConfig{}, 42);
    bus.publish(AlarmTriggered{AlarmReason::Shot});
    bus.dispatch();
    EXPECT_NEAR(sequence.shakeAmplitude(false), 7.68, 0.00001);
    EXPECT_NEAR(sequence.shakeAmplitude(true), 3.84, 0.00001);
    sequence.advance(0.2f);
    EXPECT_NEAR(sequence.paletteBlend(), 0.5, 0.00001);
    EXPECT_GT(sequence.barsFraction(), 0);
    EXPECT_GT(sequence.vignettePulse(), 0);
    sequence.advance(0.4f);
    EXPECT_NEAR(sequence.barsFraction(), 1, 0.00001);
    EXPECT_FLOAT_EQ(sequence.shakeAmplitude(false), 0);
    sequence.advance(0.2f);
    EXPECT_NEAR(sequence.barsFraction(), 1, 0.00001);
    sequence.advance(0.6f);
    EXPECT_NEAR(sequence.barsFraction(), 0.125, 0.00001);
    sequence.advance(0.6f);
    EXPECT_NEAR(sequence.barsFraction(), 0, 0.00001);
    EXPECT_FLOAT_EQ(sequence.vignettePulse(), 0);
    EXPECT_TRUE(sequence.bannerVisible());
    sequence.advance(0.5f);
    EXPECT_FALSE(sequence.bannerVisible());
}
TEST(AlarmSequence, SeededWorldShakeIsBoundedAndReducedExactlyByHalf) {
    EventBus a, b;
    AlarmSequence first(a, AlarmConfig{}, 42), second(b, AlarmConfig{}, 42);
    a.publish(AlarmTriggered{AlarmReason::Shot});
    b.publish(AlarmTriggered{AlarmReason::Shot});
    a.dispatch();
    b.dispatch();
    for (int i = 0; i < 40; ++i) {
        const auto normal = first.shakeOffset(false), reduced = first.shakeOffset(true),
                   same = second.shakeOffset(false);
        EXPECT_LE(std::hypot(normal.x, normal.y), first.shakeAmplitude(false) + 0.00001);
        EXPECT_FLOAT_EQ(normal.x, same.x);
        EXPECT_FLOAT_EQ(normal.y, same.y);
        EXPECT_FLOAT_EQ(reduced.x, normal.x * 0.5f);
        EXPECT_FLOAT_EQ(reduced.y, normal.y * 0.5f);
        first.advance(1.f / 60);
        second.advance(1.f / 60);
    }
}
TEST(AlarmSequence, ZeroDurationsDisableTheirEffectsWithoutInvalidMath) {
    EventBus bus;
    AlarmConfig config;
    config.slowmoTime = config.flipTime = config.barsIn = config.barsOut = config.bannerTime =
        config.shakePixels = 0;
    AlarmSequence sequence(bus, config, 42);
    bus.publish(AlarmTriggered{AlarmReason::Shot});
    bus.dispatch();
    sequence.advance(1.f / 60);
    EXPECT_FLOAT_EQ(sequence.paletteBlend(), 1);
    EXPECT_FLOAT_EQ(sequence.barsFraction(), 0);
    EXPECT_FLOAT_EQ(sequence.vignettePulse(), 0);
    EXPECT_FALSE(sequence.bannerVisible());
    EXPECT_FLOAT_EQ(sequence.shakeAmplitude(false), 0);
}
TEST(Config, AlarmPresentationKeysLoadAndInvalidOrMissingValuesWarnAndFallBack) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream output;
    Logger logger(output, "");
    const auto valid = Config::load("assets/config/tuning.json", logger);
    EXPECT_FLOAT_EQ(valid.alarm.traumaDecay, 1.5);
    EXPECT_FLOAT_EQ(valid.alarm.bannerTime, 2.5);
    EXPECT_FLOAT_EQ(valid.alarm.shakePixels, 12);
    data["alarm"]["trauma_decay"] = -1;
    data["alarm"]["banner_time"] = "bad";
    data["alarm"].erase("shake_pixels");
    const auto fallback = Config::load(files.write("invalid-alarm.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(fallback.alarm.traumaDecay, 1.5);
    EXPECT_FLOAT_EQ(fallback.alarm.bannerTime, 2.5);
    EXPECT_FLOAT_EQ(fallback.alarm.shakePixels, 12);
    EXPECT_NE(output.str().find("[WARN]"), std::string::npos);
}
