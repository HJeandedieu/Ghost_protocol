#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/GuardAI.h"
#include "entities/Player.h"
#include "systems/DetectionSystem.h"
#include "systems/VisionSystem.h"

class TakedownTest : public testing::Test {
   protected:
    TileMap map;
    EventBus events;
    std::ostringstream console;
    Logger logger{console, ""};
    Player player{{72, 72}, PlayerConfig{}};
    std::vector<Guard> guards;
    void SetUp() override {
        std::istringstream source("..........\n..........\n..........\n..........\n..........\n");
        map = TileMap::parse(source, 48);
        addGuard("G01", {1, 1}, true);
    }
    void addGuard(const std::string& id, TileCoord point, bool pager = false,
                  const GuardConfig& config = {}) {
        GuardSpawn spawn;
        spawn.id = id;
        spawn.mode = PatrolMode::Stationary;
        spawn.pager = pager;
        spawn.waypoints = {point};
        guards.emplace_back(spawn, map, config);
    }
};

TEST_F(TakedownTest, InclusiveRangeAnyAngleAndPagerEventOnce) {
    int takenDown = 0;
    events.subscribe<GuardTakenDown>([&](const auto& event) {
        EXPECT_EQ(event.guardId, "G01");
        EXPECT_TRUE(event.hasPager);
        ++takenDown;
    });
    player.pos = {22, 72};
    ASSERT_TRUE(player.tryTakedown(guards, events));
    EXPECT_EQ(guards.front().state(), GuardState::Unconscious);
    EXPECT_FALSE(player.tryTakedown(guards, events));
    events.dispatch();
    EXPECT_EQ(takenDown, 1);
    const auto position = guards.front().pos;
    GuardAI ai(guards.front(), map, events, logger);
    events.publish(NoiseEmitted{{120, 72}, 280, NoiseType::Step, "Ghost"});
    events.dispatch();
    ai.update(10);
    EXPECT_FLOAT_EQ(guards.front().pos.x, position.x);
    EXPECT_FLOAT_EQ(guards.front().pos.y, position.y);
    EXPECT_EQ(guards.front().state(), GuardState::Unconscious);
    EXPECT_FALSE(VisionSystem{}.sees(guards.front(), player, map));
}

TEST_F(TakedownTest, OutOfRangeFailsAndConfiguredRangeIsUsed) {
    player.pos = {122.01f, 72};
    EXPECT_FALSE(player.tryTakedown(guards, events));
    GuardConfig config;
    config.takedownRange = 70;
    guards.clear();
    addGuard("G01", {1, 1}, false, config);
    player.pos = {72, 142};
    EXPECT_TRUE(player.tryTakedown(guards, events));
}

TEST_F(TakedownTest, OnePressTakesDownOnlyNearestEligibleGuard) {
    addGuard("G02", {2, 1});
    player.pos = {110, 72};
    EXPECT_TRUE(player.tryTakedown(guards, events));
    EXPECT_EQ(guards[0].state(), GuardState::Patrol);
    EXPECT_EQ(guards[1].state(), GuardState::Unconscious);
    EXPECT_TRUE(player.tryTakedown(guards, events));
    EXPECT_EQ(guards[0].state(), GuardState::Unconscious);
}

TEST_F(TakedownTest, ActiveCallInIsCancelledAndNeverCompletes) {
    DetectionSystem detection(events, logger, guards);
    detection.update(1, player, map, guards);
    events.dispatch();
    ASSERT_EQ(guards.front().state(), GuardState::Alerted);
    int cancelled = 0;
    events.subscribe<CallInCancelled>([&](const auto& event) {
        EXPECT_EQ(event.sourceId, "G01");
        EXPECT_EQ(event.sourceType, CallInSource::Guard);
        EXPECT_EQ(event.reason, CallInCancelReason::Takedown);
        ++cancelled;
    });
    ASSERT_TRUE(player.tryTakedown(guards, events));
    detection.update(20, player, map, guards);
    events.dispatch();
    EXPECT_EQ(cancelled, 1);
    EXPECT_FLOAT_EQ(guards.front().callInRemaining(), 0);
    EXPECT_EQ(console.str().find("call-in completed"), std::string::npos);
}

TEST_F(TakedownTest, DiscoveryStartsOneConfiguredCallInWithoutPlayerSight) {
    GuardConfig config;
    config.callin = 2;
    addGuard("G02", {0, 1}, false, config);
    ASSERT_TRUE(player.tryTakedown(guards, events));
    player.pos = {400, 200};
    DetectionSystem detection(events, logger, guards);
    int found = 0, calls = 0;
    events.subscribe<BodyFound>([&](const auto& event) {
        EXPECT_EQ(event.guardId, "G02");
        EXPECT_EQ(event.bodyId, "G01");
        ++found;
    });
    events.subscribe<CallInStarted>([&](const auto& event) {
        EXPECT_EQ(event.sourceId, "G02");
        EXPECT_EQ(event.sourceType, CallInSource::Guard);
        EXPECT_FLOAT_EQ(event.seconds, 2);
        ++calls;
    });
    VisionSystem vision;
    vision.findBodies(guards, map, events);
    events.dispatch();
    EXPECT_EQ(guards[1].state(), GuardState::Alerted);
    EXPECT_FLOAT_EQ(guards[1].callInRemaining(), 2);
    vision.findBodies(guards, map, events);
    events.dispatch();
    EXPECT_EQ(found, 1);
    EXPECT_EQ(calls, 1);
    player.pos = guards[1].pos;
    EXPECT_TRUE(player.tryTakedown(guards, events));
    detection.update(10, player, map, guards);
    EXPECT_EQ(console.str().find("call-in completed"), std::string::npos);
}

TEST_F(TakedownTest, BodiesRequireConeRangeAndLineOfSightButNotReveal) {
    guards.clear();
    addGuard("Body", {3, 1});
    addGuard("Finder", {1, 1});
    ASSERT_TRUE(guards[0].takeDown(events));
    int found = 0;
    events.subscribe<BodyFound>([&](const auto&) { ++found; });
    VisionSystem vision;
    std::istringstream source("..........\n..S.......\n..........\n..........\n..........\n");
    map = TileMap::parse(source, 48);
    vision.findBodies(guards, map, events);
    events.dispatch();
    EXPECT_EQ(found, 0);
    map.setOpen(2, 1, true);
    guards[0].pos = {24, 72};
    vision.findBodies(guards, map, events);
    events.dispatch();
    EXPECT_EQ(found, 0);
    guards[0].pos = {300, 72};
    vision.findBodies(guards, map, events);
    events.dispatch();
    EXPECT_EQ(found, 0);
    map.setLight(6, 1, LightLevel::Lit);
    vision.findBodies(guards, map, events);
    events.dispatch();
    EXPECT_EQ(found, 1);
}

TEST_F(TakedownTest, QueuedDiscoveryCannotRestartCallInAfterFinderTakedown) {
    addGuard("G02", {0, 1});
    guards[0].takeDown(events);
    DetectionSystem detection(events, logger, guards);
    VisionSystem{}.findBodies(guards, map, events);
    guards[1].takeDown(events);
    events.dispatch();
    EXPECT_EQ(guards[1].state(), GuardState::Unconscious);
    EXPECT_FLOAT_EQ(guards[1].callInRemaining(), 0);
}

TEST_F(TakedownTest, InputEdgeIsConsumedOnce) {
    Input input;
    input.takedownPressed = true;
    input.clearEdges();
    EXPECT_FALSE(input.takedownPressed);
}
