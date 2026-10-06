#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "systems/DetectionSystem.h"

class DetectionSystemTest : public testing::Test {
   protected:
    TileMap map;
    EventBus events;
    std::ostringstream console;
    Logger logger{console, ""};
    Player player{{72, 72}, PlayerConfig{}};
    std::vector<Guard> guards;
    int suspicious = 0;
    int spotted = 0;
    int callIns = 0;
    float announcedSeconds = 0;
    void SetUp() override {
        std::string rows;
        for (int y = 0; y < 12; ++y) rows += std::string(16, '.') + '\n';
        std::istringstream source(rows);
        map = TileMap::parse(source, 48);
        GuardSpawn spawn;
        spawn.id = "G01";
        spawn.mode = PatrolMode::Loop;
        spawn.waypoints = {{1, 1}, {5, 1}};
        guards.emplace_back(spawn, map, GuardConfig{});
        events.subscribe<GuardSuspicious>([this](const auto& event) {
            EXPECT_EQ(event.guardId, "G01");
            EXPECT_FLOAT_EQ(event.point.x, player.pos.x);
            ++suspicious;
        });
        events.subscribe<GuardSpotted>([this](const auto& event) {
            EXPECT_EQ(event.guardId, "G01");
            ++spotted;
        });
        events.subscribe<CallInStarted>([this](const auto& event) {
            EXPECT_EQ(event.guardId, "G01");
            announcedSeconds = event.seconds;
            ++callIns;
        });
    }
};

TEST_F(DetectionSystemTest, FillsAtNearMidpointAndFarRatesForTargetLighting) {
    DetectionSystem detection(events, logger, guards);
    detection.update(0.1f, player, map, guards);
    EXPECT_FLOAT_EQ(guards.front().detection(), 10);
    EXPECT_EQ(guards.front().state(), GuardState::Suspicious);
    events.dispatch();
    EXPECT_EQ(suspicious, 1);
    for (const auto light : {LightLevel::Lit, LightLevel::Dim, LightLevel::Dark}) {
        const auto& config = guards.front().visionConfig();
        const float range = light == LightLevel::Lit   ? config.rangeLit
                            : light == LightLevel::Dim ? config.rangeDim
                                                       : config.rangeDark;
        player.pos = {72 + range * 0.5f, 72};
        map.setLight(static_cast<int>(player.pos.x / 48), 1, light);
        const float before = guards.front().detection();
        detection.update(0.1f, player, map, guards);
        EXPECT_NEAR(guards.front().detection() - before, 6.75f, 0.0001f);
        player.pos = {72 + range, 72};
        map.setLight(static_cast<int>(player.pos.x / 48), 1, light);
        const float midpoint = guards.front().detection();
        detection.update(0.1f, player, map, guards);
        EXPECT_NEAR(guards.front().detection() - midpoint, 3.5f, 0.0001f);
    }
    events.dispatch();
    EXPECT_EQ(suspicious, 1);
    EXPECT_EQ(spotted, 0);
}

TEST_F(DetectionSystemTest, SprintCrouchAndDifficultyMultiplyFillButNotDecay) {
    DetectionSystem detection(events, logger, guards, 0.75f);
    Input input;
    input.sprintHeld = true;
    player.update(0.01f, input, map);
    detection.update(0.1f, player, map, guards);
    EXPECT_NEAR(guards.front().detection(), 9.375f, 0.0001f);
    input.crouchPressed = true;
    player.update(0.01f, input, map);
    detection.update(0.1f, player, map, guards);
    EXPECT_NEAR(guards.front().detection(), 14.625f, 0.0001f);
    player.pos = {24, 72};
    detection.update(0.1f, player, map, guards);
    EXPECT_NEAR(guards.front().detection(), 12.125f, 0.0001f);
}

TEST_F(DetectionSystemTest, SuspicionStopsPatrolTurnsTowardPlayerAndDecaysToCalm) {
    DetectionSystem detection(events, logger, guards);
    player.pos = {172, 100};
    detection.update(0.1f, player, map, guards);
    auto& guard = guards.front();
    EXPECT_GT(guard.facing(), 0);
    guard.update(1, map);
    EXPECT_FLOAT_EQ(guard.pos.x, 72);
    EXPECT_FLOAT_EQ(guard.prevPos.x, 72);
    player.pos = {24, 72};
    detection.update(0.4f, player, map, guards);
    EXPECT_FLOAT_EQ(guard.detection(), 0);
    EXPECT_EQ(guard.state(), GuardState::Suspicious);
    detection.update(1, player, map, guards);
    EXPECT_EQ(guard.state(), GuardState::Patrol);
    guard.update(0.1f, map);
    EXPECT_GT(guard.pos.x, 72);
}

TEST_F(DetectionSystemTest, CallInStartsOnceAndContinuesAfterLosingSight) {
    DetectionSystem detection(events, logger, guards);
    detection.update(1, player, map, guards);
    events.dispatch();
    auto& guard = guards.front();
    EXPECT_FLOAT_EQ(guard.detection(), 100);
    EXPECT_EQ(guard.state(), GuardState::Alerted);
    EXPECT_FLOAT_EQ(guard.callInRemaining(), 3);
    EXPECT_EQ(spotted, 1);
    EXPECT_EQ(callIns, 1);
    EXPECT_FLOAT_EQ(announcedSeconds, 3);
    EXPECT_NE(console.str().find("call-in started"), std::string::npos);
    player.pos = {24, 72};
    detection.update(2.5f, player, map, guards);
    EXPECT_FLOAT_EQ(guard.callInRemaining(), 0.5f);
    EXPECT_EQ(console.str().find("call-in completed"), std::string::npos);
    detection.update(0.5f, player, map, guards);
    EXPECT_FLOAT_EQ(guard.callInRemaining(), 0);
    const auto completed = console.str();
    EXPECT_NE(completed.find("call-in completed"), std::string::npos);
    detection.update(10, player, map, guards);
    events.dispatch();
    EXPECT_EQ(console.str(), completed);
    EXPECT_EQ(spotted, 1);
    EXPECT_EQ(callIns, 1);
}

TEST_F(DetectionSystemTest, LargeTickCarriesOnlyPostSpotTimeIntoCallIn) {
    DetectionSystem detection(events, logger, guards);
    detection.update(2, player, map, guards);
    EXPECT_FLOAT_EQ(guards.front().callInRemaining(), 2);
    detection.update(20, player, map, guards);
    EXPECT_FLOAT_EQ(guards.front().callInRemaining(), 0);
    EXPECT_FLOAT_EQ(guards.front().detection(), 100);
}

TEST_F(DetectionSystemTest, CrouchedDarkFillUsesReducedRangeAndConfiguredCallIn) {
    GuardConfig config;
    config.callin = 2;
    config.fillFar = 20;
    config.fillNear = 80;
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{1, 1}};
    guards.clear();
    guards.emplace_back(spawn, map, config);
    DetectionSystem detection(events, logger, guards);
    Input input;
    input.crouchPressed = true;
    player.update(0.01f, input, map);
    player.pos = {72 + config.rangeDark * config.crouchDarkMult, 72};
    detection.update(1, player, map, guards);
    EXPECT_NEAR(guards.front().detection(), config.fillFar * config.crouchMult, 0.0001f);
    events.dispatch();
    player.pos = guards.front().pos;
    detection.update((100 - guards.front().detection()) / (config.fillNear * config.crouchMult),
                     player, map, guards);
    events.dispatch();
    EXPECT_EQ(guards.front().state(), GuardState::Alerted);
    EXPECT_NEAR(guards.front().callInRemaining(), config.callin, 0.0001f);
    EXPECT_FLOAT_EQ(announcedSeconds, config.callin);
}

TEST_F(DetectionSystemTest, OcclusionAndOutsideConeOrRangeDoNotFill) {
    DetectionSystem detection(events, logger, guards);
    player.pos = {300, 72};
    detection.update(1, player, map, guards);
    player.pos = {24, 72};
    detection.update(1, player, map, guards);
    std::istringstream source(".....\n..S..\n.....");
    map = TileMap::parse(source, 48);
    player.pos = {168, 72};
    detection.update(1, player, map, guards);
    EXPECT_FLOAT_EQ(guards.front().detection(), 0);
    EXPECT_EQ(guards.front().state(), GuardState::Patrol);
    events.dispatch();
    EXPECT_EQ(suspicious, 0);
    map.setOpen(2, 1, true);
    detection.update(0.1f, player, map, guards);
    EXPECT_GT(guards.front().detection(), 0);
}

TEST_F(DetectionSystemTest, InvalidTimeIsIgnoredAndGuardsKeepIndependentMeters) {
    DetectionSystem detection(events, logger, guards);
    GuardSpawn spawn;
    spawn.id = "G02";
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{10, 8}};
    guards.emplace_back(spawn, map, GuardConfig{});
    for (const float dt : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity()})
        detection.update(dt, player, map, guards);
    EXPECT_FLOAT_EQ(guards.front().detection(), 0);
    detection.update(0.1f, player, map, guards);
    EXPECT_FLOAT_EQ(guards.front().detection(), 10);
    EXPECT_FLOAT_EQ(guards.back().detection(), 0);
}
