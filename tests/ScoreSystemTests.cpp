#include <gtest/gtest.h>

#include <limits>

#include "core/Rng.h"
#include "systems/ScoreSystem.h"
TEST(ScoreSystem, EachPayoutLineUsesDeliveredCashAndOrderedBonuses) {
    PayoutConfig config;
    config.deductionMaxCount = 0;
    ScoreSystem score(config);
    score.addBag(config.bag);
    score.addBag(config.spoiled);
    Rng rng(7);
    const auto p = score.finalize(true, 599, 1, rng);
    EXPECT_DOUBLE_EQ(p.subtotal, 30000);
    EXPECT_NEAR(p.ghostBonus, 7500, .01);
    EXPECT_DOUBLE_EQ(p.timeBonus, 10000);
    EXPECT_NEAR(p.handlerCut, 7125, .01);
    EXPECT_DOUBLE_EQ(p.deathPenalty, 10000);
    EXPECT_NEAR(p.finalAmount, 30375, .01);
    EXPECT_EQ(p.rank, 'C');
}
TEST(ScoreSystem, TimeLimitIsExclusiveAndAlarmRemovesGhostBonus) {
    PayoutConfig config;
    config.deductionMaxCount = 0;
    ScoreSystem score(config);
    score.addBag(20000);
    Rng rng(8);
    auto p = score.finalize(false, 600, 0, rng);
    EXPECT_DOUBLE_EQ(p.ghostBonus, 0);
    EXPECT_DOUBLE_EQ(p.timeBonus, 0);
    EXPECT_DOUBLE_EQ(score.finalize(false, 599.999999, 0, rng).timeBonus, config.timeBonus);
    EXPECT_NEAR(p.finalAmount, 17000, .01);
    EXPECT_DOUBLE_EQ(
        score.finalize(true, std::numeric_limits<float>::quiet_NaN(), 0, rng).timeBonus, 0);
}
TEST(ScoreSystem, SeededDeductionsMatchInclusiveRollsAndSubtractEveryLine) {
    PayoutConfig config;
    ScoreSystem score(config);
    score.addBag(200000);
    bool zero = false, three = false;
    for (unsigned seed = 0; seed < 32; ++seed) {
        Rng expected(seed), actual(seed);
        const int count = expected.uniformInt(0, 3);
        auto p = score.finalize(false, 600, 0, actual);
        ASSERT_EQ(p.deductions.size(), static_cast<std::size_t>(count));
        double total = 0;
        for (int i = 0; i < count; ++i) {
            const int amount = expected.uniformInt(0, 500);
            EXPECT_EQ(p.deductions[i], amount);
            EXPECT_GE(amount, 0);
            EXPECT_LE(amount, 500);
            total += amount;
        }
        EXPECT_NEAR(p.finalAmount, 170000 - total, .02);
        zero |= count == 0;
        three |= count == 3;
    }
    EXPECT_TRUE(zero);
    EXPECT_TRUE(three);
}
TEST(ScoreSystem, DeathPenaltyClampsPayoutAndInvalidBagsDoNotCredit) {
    PayoutConfig config;
    config.deductionMaxCount = 0;
    ScoreSystem score(config);
    score.addBag(-100);
    score.addBag(std::numeric_limits<float>::infinity());
    score.addBag(10000);
    Rng rng(1);
    auto p = score.finalize(false, 600, 2, rng);
    EXPECT_DOUBLE_EQ(p.subtotal, 10000);
    EXPECT_DOUBLE_EQ(p.deathPenalty, 20000);
    EXPECT_DOUBLE_EQ(p.finalAmount, 0);
}
TEST(ScoreSystem, RankBoundariesUseFinalPayout) {
    ScoreSystem score(PayoutConfig{});
    EXPECT_EQ(score.rank(200000), 'S');
    EXPECT_EQ(score.rank(199999.99), 'A');
    EXPECT_EQ(score.rank(150000), 'A');
    EXPECT_EQ(score.rank(149999.99), 'B');
    EXPECT_EQ(score.rank(90000), 'B');
    EXPECT_EQ(score.rank(89999.99), 'C');
}
TEST(ScoreSystem, MissionClockRetainsActiveRealTimeAndStopsOnBusted) {
    MissionRun run;
    run.advance(.8f, true);
    run.deaths++;
    run.alarmEver = true;
    run.advance(10, false);
    run.advance(2, true);
    run.advance(-1, true);
    EXPECT_NEAR(run.seconds, 2.8, 1e-6);
    EXPECT_EQ(run.deaths, 1);
    EXPECT_TRUE(run.alarmEver);
    MissionRun fresh;
    EXPECT_EQ(fresh.seconds, 0);
    EXPECT_EQ(fresh.deaths, 0);
    EXPECT_FALSE(fresh.alarmEver);
}

TEST(ScoreSystem, HandlerRatePrecisionPreservesExactNormalRankBoundary) {
    PayoutConfig config;
    config.deductionMaxCount = 0;
    ScoreSystem score(config);
    for (int i = 0; i < 10; ++i) score.addBag(config.bag);
    Rng rng(42);
    const auto payout = score.finalize(false, 600, 2, rng);
    EXPECT_DOUBLE_EQ(payout.handlerCut, 30000);
    EXPECT_DOUBLE_EQ(payout.finalAmount, 150000);
    EXPECT_EQ(payout.rank, 'A');
}
