#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "world/Pathfinder.h"
#include "world/Raycast.h"
#include "world/TileMap.h"

TEST(Pathfinder, GoesAroundWallAndEveryLegClearsGuardRadius) {
    std::istringstream source(".......\n...#...\n...#...\n...#...\n.......\n");
    const auto map = TileMap::parse(source, 48);
    const Vec2 from{72, 120}, to{264, 120};
    ASSERT_FALSE(Raycast::isPathClear(from, to, 14, map));
    const auto path = Pathfinder{}.findPath(from, to, map);
    ASSERT_FALSE(path.empty());
    Vec2 previous = from;
    for (const auto point : path) {
        EXPECT_TRUE(Raycast::isPathClear(previous, point, 14, map));
        previous = point;
    }
    EXPECT_FLOAT_EQ(path.back().x, to.x);
    EXPECT_FLOAT_EQ(path.back().y, to.y);
}

TEST(Pathfinder, SealedWallClosedSpecialDoorsAndInvalidEndpointsReturnEmpty) {
    std::istringstream source("..#..\n..S..\n..#..\n");
    auto map = TileMap::parse(source, 48);
    Pathfinder pathfinder;
    EXPECT_TRUE(pathfinder.findPath({72, 72}, {168, 72}, map).empty());
    map.setOpen(2, 1, true);
    EXPECT_FALSE(pathfinder.findPath({72, 72}, {168, 72}, map).empty());
    EXPECT_TRUE(pathfinder.findPath({-1, 72}, {168, 72}, map).empty());
    EXPECT_TRUE(
        pathfinder.findPath({72, 72}, {std::numeric_limits<float>::infinity(), 72}, map).empty());
}

TEST(Pathfinder, NormalDoorsArePassableButDiagonalCornerCuttingIsRejected) {
    std::istringstream doors("..#..\n..d..\n..#..\n");
    const auto map = TileMap::parse(doors, 48);
    EXPECT_FALSE(Pathfinder{}.findPath({72, 72}, {168, 72}, map).empty());
    std::istringstream corners(".#\n#.\n");
    const auto blocked = TileMap::parse(corners, 48);
    EXPECT_TRUE(Pathfinder{}.findPath({24, 24}, {72, 72}, blocked).empty());
}

TEST(Pathfinder, SamePositionSucceedsAndNarrowCorridorRespectsConfiguredRadius) {
    std::istringstream source("#####\n.....\n#####\n");
    const auto map = TileMap::parse(source, 24);
    EXPECT_TRUE(Pathfinder{}.findPath({36, 36}, {84, 36}, map).empty());
    GuardConfig config;
    config.radius = 10;
    EXPECT_FALSE(Pathfinder(config).findPath({36, 36}, {84, 36}, map).empty());
    EXPECT_EQ(Pathfinder(config).findPath({36, 36}, {36, 36}, map).size(), 1u);
}
