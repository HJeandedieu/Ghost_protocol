#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/GuardAI.h"
#include "entities/Player.h"
#include "systems/DetectionSystem.h"
#include "systems/NoiseSystem.h"

class GuardAITest : public testing::Test {
   protected:
    TileMap map;
    GuardConfig config;
    EventBus events;
    std::ostringstream console;
    Logger logger{console, ""};
    std::unique_ptr<Guard> guard;
    std::unique_ptr<GuardAI> ai;
    void load(std::string rows = "", int size = 48) {
        if (rows.empty())
            for (int y = 0; y < 16; ++y) rows += std::string(20, '.') + '\n';
        std::istringstream source(rows);
        map = TileMap::parse(source, size);
        GuardSpawn spawn;
        spawn.id = "G01";
        spawn.mode = PatrolMode::Stationary;
        spawn.facing = 180;
        spawn.waypoints = {{5, 5}};
        guard = std::make_unique<Guard>(spawn, map, config);
        ai = std::make_unique<GuardAI>(*guard, map, events, logger);
    }
    void SetUp() override { load(); }
    void hear(Vec2 point, float radius = 280) {
        events.publish(NoiseEmitted{point, radius, NoiseType::Step, "Ghost"});
        events.dispatch();
    }
    void tickUntil(GuardState state, float limit = 20) {
        for (float time = 0; time < limit && guard->state() != state; time += 1.0f / 60)
            ai->update(1.0f / 60);
        ASSERT_EQ(guard->state(), state);
    }
};

TEST_F(GuardAITest, SprintIsHeardCrouchIsNotAndOnlyNearbyGuardsReact) {
    const Vec2 point{guard->pos.x + 100, guard->pos.y};
    hear(point, 40);
    EXPECT_EQ(guard->state(), GuardState::Patrol);
    GuardSpawn spawn;
    spawn.id = "G02";
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{15, 12}};
    Guard far(spawn, map, config);
    GuardAI farAi(far, map, events, logger);
    hear(point, 280);
    EXPECT_EQ(guard->state(), GuardState::Suspicious);
    EXPECT_EQ(far.state(), GuardState::Patrol);
    EXPECT_FLOAT_EQ(guard->facing(), 0);
    ai->update(1);
    EXPECT_FLOAT_EQ(guard->pos.x, 264);
    ai->update(0.5f);
    EXPECT_EQ(guard->state(), GuardState::Investigating);
    ai->update(0.2f);
    EXPECT_NEAR(guard->pos.x, 290, 0.001f);
}

TEST_F(GuardAITest, WallDoesNotBlockHearingButPreventsInvestigation) {
    std::string rows;
    for (int y = 0; y < 16; ++y) {
        std::string row(20, '.');
        row[6] = '#';
        rows += row + '\n';
    }
    // Replace the AI only after its event bus is reset to remove the old callback.
    ai.reset();
    events = EventBus{};
    load(rows);
    hear({360, 264});
    EXPECT_EQ(guard->state(), GuardState::Suspicious);
    EXPECT_FALSE(ai->canReach({360, 264}));
    ai->update(config.suspiciousTime);
    EXPECT_EQ(guard->state(), GuardState::Patrol);
    EXPECT_FLOAT_EQ(guard->pos.x, 264);
    EXPECT_NEAR(guard->facing(), 3.14159265358979323846f, 0.0001f);
}

TEST_F(GuardAITest, NormalDoorIsReachableAndOpensDuringInvestigation) {
    std::string rows;
    for (int y = 0; y < 16; ++y) {
        std::string row(20, '.');
        if (y == 5) row[6] = 'd';
        rows += row + '\n';
    }
    ai.reset();
    events = EventBus{};
    load(rows);
    EXPECT_TRUE(ai->canReach({360, 264}));
    hear({360, 264});
    ai->update(config.suspiciousTime + 0.5f);
    EXPECT_TRUE(map.isOpen(6, 5));
    EXPECT_GT(guard->pos.x, 300);
}

TEST_F(GuardAITest, NoiseBehindWallIsReachedAroundItAndBreadcrumbsReturn) {
    std::string rows;
    for (int y = 0; y < 16; ++y) {
        std::string row(20, '.');
        if (y >= 3 && y <= 7) row[6] = '#';
        rows += row + '\n';
    }
    ai.reset();
    events = EventBus{};
    load(rows);
    const auto route = guard->pos;
    const Vec2 target{360, 264};
    EXPECT_TRUE(ai->canReach(target));
    hear(target);
    tickUntil(GuardState::Investigating);
    tickUntil(GuardState::Searching);
    EXPECT_NEAR(guard->pos.x, target.x, config.arriveTolerance);
    EXPECT_NEAR(guard->pos.y, target.y, config.arriveTolerance);
    tickUntil(GuardState::Patrol, 30);
    EXPECT_FLOAT_EQ(guard->pos.x, route.x);
    EXPECT_FLOAT_EQ(guard->pos.y, route.y);
}

TEST_F(GuardAITest, ArrivalSweepsThenSearchesClockwiseAndReturnsToRoute) {
    const auto route = guard->pos;
    hear({312, 264});
    tickUntil(GuardState::Investigating);
    ai->update(0.33f);
    const auto arrived = guard->pos;
    const float heading = guard->facing();
    ai->update(config.investigateLook * 0.25f);
    EXPECT_FLOAT_EQ(guard->pos.x, arrived.x);
    EXPECT_NEAR(guard->facing() - heading, config.lookSweepDeg * 3.14159265358979323846f / 180,
                0.15f);
    tickUntil(GuardState::Searching);
    ASSERT_EQ(ai->searchPoints().size(), 4u);
    // Arrival was west of the centre: west is closest, followed by north, east, south.
    EXPECT_FLOAT_EQ(ai->searchPoints()[0].x, 216);
    EXPECT_FLOAT_EQ(ai->searchPoints()[1].y, 168);
    EXPECT_FLOAT_EQ(ai->searchPoints()[2].x, 408);
    EXPECT_FLOAT_EQ(ai->searchPoints()[3].y, 360);
    ai->update(7.9f);
    EXPECT_EQ(guard->state(), GuardState::Searching);
    ai->update(0.101f);
    EXPECT_EQ(guard->state(), GuardState::Returning);
    tickUntil(GuardState::Patrol);
    EXPECT_FLOAT_EQ(guard->pos.x, route.x);
    EXPECT_FLOAT_EQ(guard->pos.y, route.y);
    EXPECT_NEAR(guard->facing(), 3.14159265358979323846f, 0.01f);
}

TEST_F(GuardAITest, SearchFiltersBlockedCandidatesAndUsesCentreBetweenBlockedLegs) {
    std::string rows;
    for (int y = 0; y < 16; ++y) {
        std::string row(20, '.');
        if (y == 5) row[7] = '#';  // East candidate from (264,264) is blocked.
        if (y == 6) row[4] = '#';  // West-to-south diagonal leg crosses a wall.
        rows += row + '\n';
    }
    ai.reset();
    events = EventBus{};
    load(rows);
    hear(guard->pos);
    tickUntil(GuardState::Searching);
    ASSERT_EQ(ai->searchPoints().size(), 3u);
    EXPECT_FLOAT_EQ(ai->searchPoints()[0].y, 360);
    EXPECT_FLOAT_EQ(ai->searchPoints()[1].x, 168);
    EXPECT_FLOAT_EQ(ai->searchPoints()[2].y, 168);
    ai->update(7.9f);
    EXPECT_EQ(guard->state(), GuardState::Searching);
    EXPECT_GE(guard->pos.x, 14);
    tickUntil(GuardState::Patrol);
    EXPECT_FLOAT_EQ(guard->pos.x, 264);
    EXPECT_FLOAT_EQ(guard->pos.y, 264);
}

TEST_F(GuardAITest, NoSearchCandidatesRotatesInPlaceUntilTimeout) {
    config.searchLoopRadius = 2000;
    ai.reset();
    events = EventBus{};
    load();
    hear(guard->pos);
    tickUntil(GuardState::Searching);
    ASSERT_TRUE(ai->searchPoints().empty());
    const auto start = guard->pos;
    const auto heading = guard->facing();
    ai->update(1);
    EXPECT_FLOAT_EQ(guard->pos.x, start.x);
    EXPECT_NEAR(guard->facing() - heading, config.searchTurnRate * 3.14159265358979323846f / 180,
                0.001f);
    ai->update(7);
    EXPECT_EQ(guard->state(), GuardState::Patrol);
}

TEST_F(GuardAITest, ClosedDoorAfterReachabilityCheckTriggersStuckSearchAtCurrentPosition) {
    std::string rows;
    for (int y = 0; y < 16; ++y) {
        std::string row(20, '.');
        if (y == 5) row[6] = 'S';
        rows += row + '\n';
    }
    ai.reset();
    events = EventBus{};
    load(rows);
    map.setOpen(6, 5, true);
    hear({408, 264});
    ai->update(config.suspiciousTime);
    ASSERT_EQ(guard->state(), GuardState::Investigating);
    map.setOpen(6, 5, false);
    tickUntil(GuardState::Searching, 4);
    EXPECT_LE(guard->pos.x, 274);
    ASSERT_FALSE(ai->searchPoints().empty());
    // East is blocked: first candidate is south of the stuck position.
    EXPECT_NEAR(ai->searchPoints()[0].x, guard->pos.x, 0.001f);
    EXPECT_FLOAT_EQ(ai->searchPoints()[0].y, 360);
}

TEST_F(GuardAITest, NewNoiseInterruptsSearchAndReturnWithoutLosingOriginalRoute) {
    const auto route = guard->pos;
    hear({360, 264});
    tickUntil(GuardState::Searching);
    ai->update(1);
    hear({guard->pos.x, guard->pos.y + 48});
    EXPECT_EQ(guard->state(), GuardState::Suspicious);
    tickUntil(GuardState::Returning);
    hear({guard->pos.x, guard->pos.y});
    EXPECT_EQ(guard->state(), GuardState::Suspicious);
    tickUntil(GuardState::Patrol, 30);
    EXPECT_FLOAT_EQ(guard->pos.x, route.x);
    EXPECT_FLOAT_EQ(guard->pos.y, route.y);
}

TEST_F(GuardAITest, DetectionInterruptsInvestigationAndNoiseCannotCancelCallIn) {
    hear({360, 264});
    ai->update(config.suspiciousTime + 0.1f);
    ASSERT_EQ(guard->state(), GuardState::Investigating);
    Player player(guard->pos, PlayerConfig{});
    std::vector<Guard> copies{*guard};
    DetectionSystem detection(events, logger, copies);
    detection.update(0.1f, player, map, copies);
    EXPECT_EQ(copies.front().state(), GuardState::Suspicious);
    detection.update(1, player, map, copies);
    EXPECT_EQ(copies.front().state(), GuardState::Alerted);
    GuardAI copyAi(copies.front(), map, events, logger);
    events.publish(NoiseEmitted{copies.front().pos, 280, NoiseType::Step, "Ghost"});
    events.dispatch();
    EXPECT_EQ(copies.front().state(), GuardState::Alerted);
}

TEST_F(GuardAITest, InvalidTimeAndNoiseDoNotChangeState) {
    hear({std::numeric_limits<float>::quiet_NaN(), 264});
    hear(guard->pos, -1);
    for (const auto dt : {0.0f, -1.0f, std::numeric_limits<float>::infinity()}) ai->update(dt);
    EXPECT_EQ(guard->state(), GuardState::Patrol);
    EXPECT_FLOAT_EQ(guard->pos.x, 264);
}

TEST_F(GuardAITest, NoiseSystemEmitsThroughEventBusAndNotifiesOnce) {
    NoiseSystem noise(events);
    noise.setHearers({{guard->id, guard->pos}});
    int notifications = 0;
    events.subscribe<GuardSuspicious>([&](const auto& event) {
        EXPECT_EQ(event.guardId, guard->id);
        ++notifications;
    });
    noise.emit({guard->pos.x + 100, guard->pos.y}, 280, NoiseType::Step, "Ghost");
    EXPECT_EQ(guard->state(), GuardState::Patrol);
    events.dispatch();
    EXPECT_EQ(guard->state(), GuardState::Suspicious);
    EXPECT_EQ(notifications, 0);
    events.dispatch();
    EXPECT_EQ(notifications, 1);
}

TEST_F(GuardAITest, MovingGuardResumesItsInterruptedWaypointAndHeading) {
    ai.reset();
    events = EventBus{};
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.mode = PatrolMode::Loop;
    spawn.waypoints = {{5, 5}, {7, 5}, {7, 7}};
    guard = std::make_unique<Guard>(spawn, map, config);
    ai = std::make_unique<GuardAI>(*guard, map, events, logger);
    ai->update(0.6f);
    const auto route = guard->pos;
    hear({route.x, route.y + 48});
    tickUntil(GuardState::Searching);
    tickUntil(GuardState::Returning);
    tickUntil(GuardState::Patrol);
    EXPECT_NEAR(guard->pos.x, route.x, 2);
    EXPECT_FLOAT_EQ(guard->pos.y, route.y);
    EXPECT_FLOAT_EQ(guard->facing(), 0);
    ai->update(0.3f);
    EXPECT_GT(guard->pos.x, route.x + 25);
    EXPECT_FLOAT_EQ(guard->pos.y, route.y);
}

TEST_F(GuardAITest, SearchHasAnExactEightSecondBudget) {
    config.investigateLook = 0;
    ai.reset();
    events = EventBus{};
    load();
    hear(guard->pos);
    ai->update(config.suspiciousTime);
    ASSERT_EQ(guard->state(), GuardState::Investigating);
    ai->update(7.5f);
    EXPECT_EQ(guard->state(), GuardState::Searching);
    ai->update(0.5f);
    EXPECT_EQ(guard->state(), GuardState::Returning);
}

TEST_F(GuardAITest, BlockedBreadcrumbReturnWarnsAndSnapsToContinue) {
    std::string rows;
    for (int y = 0; y < 16; ++y) {
        std::string row(20, '.');
        if (y == 5) row[6] = 'S';
        rows += row + '\n';
    }
    ai.reset();
    events = EventBus{};
    load(rows);
    map.setOpen(6, 5, true);
    hear({408, 264});
    tickUntil(GuardState::Searching);
    ai->update(2);
    guard->returnToRoute();
    map.setOpen(6, 5, false);
    tickUntil(GuardState::Patrol, 30);
    EXPECT_NE(console.str().find("[WARN] G01: stuck returning"), std::string::npos);
    EXPECT_FLOAT_EQ(guard->pos.x, 264);
    EXPECT_FLOAT_EQ(guard->pos.y, 264);
}
