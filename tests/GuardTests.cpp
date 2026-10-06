#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>

#include "core/Logger.h"
#include "entities/Guard.h"
#include "world/LevelLoader.h"
#include "world/World.h"

namespace {
TileMap openMap() {
    std::istringstream source(".......\n.......\n.......\n.......\n.......");
    return TileMap::parse(source, 48);
}
GuardSpawn route(PatrolMode mode) {
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.pager = true;
    spawn.mode = mode;
    spawn.waypoints = {{1, 1}, {3, 1}, {3, 3}};
    return spawn;
}
}  // namespace

TEST(Guard, LoadsTileCentersPagerFacingAndMovesAtConfiguredSpeed) {
    auto map = openMap();
    auto spawn = route(PatrolMode::Loop);
    GuardConfig config;
    Guard guard(spawn, map, config);
    EXPECT_EQ(guard.id, "G01");
    EXPECT_TRUE(guard.hasPager());
    EXPECT_FLOAT_EQ(guard.radius, 14);
    EXPECT_FLOAT_EQ(guard.pos.x, 72);
    guard.update(0.5f, map);
    EXPECT_FLOAT_EQ(guard.pos.x, 117);
    EXPECT_FLOAT_EQ(guard.pos.y, 72);
    EXPECT_FLOAT_EQ(guard.prevPos.x, 72);
    EXPECT_FLOAT_EQ(guard.interpolatedPosition(0.5f).x, 94.5f);
    EXPECT_FLOAT_EQ(guard.facing(), 0);
}

TEST(Guard, LoopAndPingPongConsumeRemainingDistanceAcrossWaypoints) {
    auto map = openMap();
    GuardConfig config;
    config.patrolSpeed = 96;
    auto spawn = route(PatrolMode::Loop);
    spawn.waypoints = {{1, 1}, {3, 1}, {3, 3}, {1, 3}};
    Guard loop(spawn, map, config);
    loop.update(4.5f, map);
    EXPECT_FLOAT_EQ(loop.pos.x, 120);
    EXPECT_FLOAT_EQ(loop.pos.y, 72);
    spawn.mode = PatrolMode::PingPong;
    spawn.waypoints = {{1, 1}, {3, 1}, {3, 3}};
    Guard pingpong(spawn, map, config);
    pingpong.update(2.5f, map);
    EXPECT_FLOAT_EQ(pingpong.pos.x, 168);
    EXPECT_FLOAT_EQ(pingpong.pos.y, 120);
    pingpong.update(2, map);
    EXPECT_FLOAT_EQ(pingpong.pos.x, 120);
    EXPECT_FLOAT_EQ(pingpong.pos.y, 72);
}

TEST(Guard, StationaryTurnsClockwiseFromInitialFacingWithoutMoving) {
    auto map = openMap();
    auto spawn = route(PatrolMode::Stationary);
    spawn.facing = 180;
    Guard guard(spawn, map, GuardConfig{});
    guard.update(3, map);
    EXPECT_NEAR(guard.facing(), 240 * 3.14159265358979323846 / 180, 0.00001);
    EXPECT_FLOAT_EQ(guard.pos.x, 72);
    EXPECT_FLOAT_EQ(guard.pos.y, 72);
}

TEST(Guard, InvalidTimeEmptyRoutesAndDuplicateWaypointsAreSafe) {
    auto map = openMap();
    auto spawn = route(PatrolMode::Loop);
    spawn.waypoints.clear();
    EXPECT_THROW(Guard(spawn, map, GuardConfig{}), std::invalid_argument);
    spawn.waypoints = {{1, 1}, {1, 1}};
    Guard guard(spawn, map, GuardConfig{});
    guard.update(1, map);
    guard.update(-1, map);
    guard.update(std::numeric_limits<float>::quiet_NaN(), map);
    EXPECT_FLOAT_EQ(guard.pos.x, 72);
    EXPECT_FLOAT_EQ(guard.pos.y, 72);
}

TEST(Guard, SolidWallStopsPatrolAndNormalDoorOpensOnContact) {
    std::istringstream source(".....\n..#..\n.....");
    auto map = TileMap::parse(source, 48);
    auto spawn = route(PatrolMode::PingPong);
    spawn.waypoints = {{1, 1}, {3, 1}};
    Guard guard(spawn, map, GuardConfig{});
    guard.update(10, map);
    EXPECT_LE(guard.pos.x + guard.radius, 96);
    std::istringstream doors(".....\n..d..\n.....");
    map = TileMap::parse(doors, 48);
    Guard doorGuard(spawn, map, GuardConfig{});
    doorGuard.update(0.25f, map);
    EXPECT_TRUE(map.isOpen(2, 1));
}

TEST(Guard, AllElevenShippedGuardsPatrolForThreeMinutesWithoutTouchingSolidTiles) {
    std::ostringstream console;
    Logger logger(console, "");
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    const auto config = Config::load("assets/config/tuning.json", logger);
    World world(std::move(*level), config.player, config.guard);
    ASSERT_EQ(world.guards.size(), 11u);
    std::vector<Vec2> starts;
    std::vector<float> travelled(world.guards.size(), 0);
    for (const auto& guard : world.guards) starts.push_back(guard.pos);
    for (int tick = 0; tick < 60 * 180; ++tick) {
        for (std::size_t i = 0; i < world.guards.size(); ++i) {
            auto& guard = world.guards[i];
            const auto before = guard.pos;
            guard.update(1.0f / 60, world.level.map);
            const float distance = std::hypot(guard.pos.x - before.x, guard.pos.y - before.y);
            travelled[i] += distance;
            if (guard.mode() != PatrolMode::Stationary) {
                EXPECT_GT(distance, 1.0f) << guard.id;
            }
            EXPECT_TRUE(world.level.map.isPassable(static_cast<int>(guard.pos.x / 48),
                                                   static_cast<int>(guard.pos.y / 48)))
                << guard.id;
        }
    }
    for (std::size_t i = 0; i < world.guards.size(); ++i) {
        if (world.guards[i].mode() == PatrolMode::Stationary) {
            EXPECT_FLOAT_EQ(world.guards[i].pos.x, starts[i].x);
            EXPECT_FLOAT_EQ(world.guards[i].pos.y, starts[i].y);
        } else {
            EXPECT_GT(travelled[i], 15000) << world.guards[i].id;
        }
    }
}
