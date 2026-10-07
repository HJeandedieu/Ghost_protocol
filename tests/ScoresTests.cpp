#include <gtest/gtest.h>

#include <limits>

#include "core/Scores.h"

TEST(Scores, ScoreRecordsKeepPayoutRankTogetherAndFastestTimeIndependently) {
    Scores scores;
    ASSERT_TRUE(scores.record("normal", 200000.25, 'S', 700, true));
    ASSERT_TRUE(scores.record("normal", 90000, 'B', 600, false));
    EXPECT_DOUBLE_EQ(scores.normal.bestPayout, 200000.25);
    EXPECT_EQ(scores.normal.bestRank, "S");
    EXPECT_DOUBLE_EQ(scores.normal.bestTime, 600);
    EXPECT_EQ(scores.normal.ghostRuns, 1u);
    ASSERT_TRUE(scores.record("normal", 200000.25, 'A', 800, true));
    EXPECT_EQ(scores.normal.bestRank, "S");
    EXPECT_EQ(scores.normal.ghostRuns, 2u);
    ASSERT_TRUE(scores.record("easy", 100, 'C', 900, false));
    EXPECT_DOUBLE_EQ(scores.easy.bestPayout, 100);
    EXPECT_EQ(scores.hard.bestRank, "-");
    EXPECT_FALSE(scores.record("other", 10, 'C', 10, true));
    EXPECT_FALSE(scores.record("normal", -1, 'C', 10, true));
    EXPECT_FALSE(scores.record("normal", 10, 'Z', 10, true));
    EXPECT_FALSE(scores.record("normal", 10, 'C', std::numeric_limits<double>::infinity(), true));
    EXPECT_EQ(scores.normal.ghostRuns, 2u);
}
TEST(Scores, InvalidCompletionPreservesRecordsAndZeroPayoutTiePreservesDefaultRank) {
    Scores scores;
    EXPECT_TRUE(scores.record("normal", 0, 'C', 100, true));
    EXPECT_EQ(scores.normal.bestRank, "-");
    EXPECT_EQ(scores.normal.ghostRuns, 1u);
    EXPECT_FALSE(scores.record("normal", std::numeric_limits<double>::quiet_NaN(), 'C', 100, true));
    EXPECT_FALSE(scores.record("normal", 100, 'C', -1, true));
    EXPECT_EQ(scores.normal.ghostRuns, 1u);
    EXPECT_DOUBLE_EQ(scores.normal.bestTime, 100);
}
