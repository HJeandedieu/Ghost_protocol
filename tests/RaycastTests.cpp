#include <gtest/gtest.h>

#include <sstream>

#include "world/Raycast.h"
#include "world/TileMap.h"

TEST(Raycast, ClearRaysWorkInBothDirectionsAndWallsBlock) {
    std::istringstream source(".....\n..#..\n.....");
    const auto map = TileMap::parse(source, 48);
    EXPECT_TRUE(Raycast::hasLineOfSight({24, 24}, {216, 24}, map));
    EXPECT_TRUE(Raycast::hasLineOfSight({216, 24}, {24, 24}, map));
    EXPECT_FALSE(Raycast::hasLineOfSight({24, 72}, {216, 72}, map));
    EXPECT_FALSE(Raycast::hasLineOfSight({216, 72}, {24, 72}, map));
    EXPECT_TRUE(Raycast::hasLineOfSight({24, 72}, {120, 72}, map, true));
    EXPECT_FALSE(Raycast::hasLineOfSight({24, 72}, {120, 72}, map));
}

TEST(Raycast, DoorsBlockAndDiagonalCornersDoNotLeak) {
    for (char symbol : std::string("dSRGVF")) {
        std::istringstream doors(std::string(".") + symbol + ".");
        const auto map = TileMap::parse(doors, 48);
        EXPECT_FALSE(Raycast::hasLineOfSight({24, 24}, {120, 24}, map)) << symbol;
    }
    std::istringstream corners(".#\n#.");
    const auto cornerMap = TileMap::parse(corners, 48);
    EXPECT_FALSE(Raycast::hasLineOfSight({24, 24}, {72, 72}, cornerMap));
    EXPECT_FALSE(Raycast::hasLineOfSight({72, 72}, {24, 24}, cornerMap));
}

TEST(Raycast, VerticalSameTileAndOutOfBoundsRaysAreSafe) {
    std::istringstream source("...\n...\n...");
    const auto map = TileMap::parse(source, 48);
    EXPECT_TRUE(Raycast::hasLineOfSight({24, 24}, {24, 120}, map));
    EXPECT_TRUE(Raycast::hasLineOfSight({24, 120}, {24, 24}, map));
    EXPECT_TRUE(Raycast::hasLineOfSight({24, 24}, {24, 24}, map));
    EXPECT_FALSE(Raycast::hasLineOfSight({-1, 24}, {24, 24}, map));
    EXPECT_FALSE(Raycast::hasLineOfSight({24, 24}, {144, 24}, map));
}

TEST(Raycast, SegmentHitsCircleAtFirstContactAndRejectsMisses) {
    const auto hit = Raycast::intersectCircle({0, 0}, {100, 0}, {50, 0}, 10);
    ASSERT_TRUE(hit);
    EXPECT_FLOAT_EQ(*hit, 0.4f);
    EXPECT_FALSE(Raycast::intersectCircle({0, 0}, {100, 0}, {50, 20}, 10));
    EXPECT_FALSE(Raycast::intersectCircle({0, 0}, {20, 0}, {50, 0}, 10));
    EXPECT_FALSE(Raycast::intersectCircle({0, 0}, {0, 0}, {50, 0}, 10));
    EXPECT_EQ(Raycast::intersectCircle({50, 0}, {50, 0}, {50, 0}, 10), 0.0f);
    EXPECT_EQ(Raycast::intersectCircle({0, 0}, {100, 0}, {50, 10}, 10), 0.5f);
}
