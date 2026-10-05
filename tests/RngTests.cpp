#include <gtest/gtest.h>

#include <stdexcept>

#include "core/Rng.h"

TEST(Rng, SameSeedReplaysTheSameMixedSequence) {
    Rng first(1234);
    Rng second(1234);
    EXPECT_EQ(first.seed(), 1234u);
    for (int sample = 0; sample < 100; ++sample) {
        EXPECT_EQ(first.uniformInt(0, 100), second.uniformInt(0, 100));
        EXPECT_FLOAT_EQ(first.uniformFloat(-1.0f, 1.0f), second.uniformFloat(-1.0f, 1.0f));
    }
}

TEST(Rng, ValuesRespectBoundsAndInvalidBoundsAreRejected) {
    Rng rng(1234);
    for (int sample = 0; sample < 100; ++sample) {
        const auto value = rng.uniformFloat(0.0f, 1.0f);
        EXPECT_GE(value, 0.0f);
        EXPECT_LE(value, 1.0f);
    }
    EXPECT_EQ(rng.uniformInt(4, 4), 4);
    EXPECT_FLOAT_EQ(rng.uniformFloat(2.0f, 2.0f), 2.0f);
    EXPECT_THROW(rng.uniformInt(4, 3), std::invalid_argument);
    EXPECT_THROW(rng.uniformFloat(2.0f, 1.0f), std::invalid_argument);
}
