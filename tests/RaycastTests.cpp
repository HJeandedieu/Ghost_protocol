#include <gtest/gtest.h>

#include <cmath>
#include <sstream>

#include "world/Raycast.h"
#include "world/TileMap.h"

TEST(Raycast, SightDistanceStopsAtWallDoorCornerAndMapBoundary) {
    std::istringstream source(".....\n..S..\n.....");
    auto map = TileMap::parse(source, 48);
    EXPECT_FLOAT_EQ(Raycast::sightDistance({72, 72}, {216, 72}, map), 24);
    map.setOpen(2, 1, true);
    EXPECT_FLOAT_EQ(Raycast::sightDistance({72, 72}, {216, 72}, map), 144);
    EXPECT_FLOAT_EQ(Raycast::sightDistance({72, 24}, {500, 24}, map), 168);
    EXPECT_FLOAT_EQ(Raycast::sightDistance({72, 24}, {24, 24}, map), 48);
    std::istringstream walls(".#.\n#..\n...");
    map = TileMap::parse(walls, 48);
    EXPECT_NEAR(Raycast::sightDistance({24, 24}, {72, 72}, map), std::sqrt(2.0f) * 24, 0.0001f);
    EXPECT_FLOAT_EQ(Raycast::sightDistance({72, 24}, {120, 24}, map), 0);
}

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
