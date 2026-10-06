#include <gtest/gtest.h>

#include <memory>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "systems/AlarmDirector.h"
#include "systems/DetectionSystem.h"
#include "systems/InteractionSystem.h"
#include "world/World.h"

class CameraTest : public testing::Test {
   protected:
    EventBus events;
    std::ostringstream console;
    Logger logger{console, ""};
    Config config;
    std::unique_ptr<World> world;
    std::unique_ptr<DetectionSystem> detection;
    std::unique_ptr<AlarmDirector> alarm;
    InteractionSystem interaction{events};
    void SetUp() override {
        Level level;
        std::string rows;
        for (int y = 0; y < 10; ++y) {
            std::string row(12, '.');
            if (y == 1) row[1] = 'P';
            rows += row + '\n';
        }
        std::istringstream source(rows);
        level.map = TileMap::parse(source, 48);
        level.playerSpawn = {1, 1};
        CameraSpawn camera;
        camera.id = "C01";
        camera.position = {1, 1};
        camera.range = config.camera.range;
        level.cameras.push_back(camera);
        GuardSpawn guard;
        guard.id = "G01";
        guard.mode = PatrolMode::Stationary;
        guard.waypoints = {{1, 1}};
        level.guards.push_back(guard);
        world =
            std::make_unique<World>(std::move(level), config.player, config.guard, config.camera);
        detection = std::make_unique<DetectionSystem>(events, logger, world->guards);
        detection->bindCameras(world->cameras);
        alarm = std::make_unique<AlarmDirector>(events, logger, *world);
        interaction.loadBank(*world, config);
    }
    void updateCamera(float dt, bool disabled = false) {
        detection->updateCameras(dt, world->player, world->level.map, world->cameras, config.guard,
                                 disabled);
    }
};

TEST_F(CameraTest, SweepReflectsAtConfiguredLimitsAndSupportsLargeTicks) {
    CameraSpawn spawn;
    spawn.id = "C02";
    spawn.position = {1, 1};
    spawn.angle = 90;
    spawn.sweep = 25;
    SecurityCamera camera(spawn, world->level.map, config.camera);
    camera.update(1.25f);
    EXPECT_FLOAT_EQ(camera.facing(), 115);
    camera.update(2.5f);
    EXPECT_FLOAT_EQ(camera.facing(), 65);
    camera.update(101.25f);
    EXPECT_FLOAT_EQ(camera.facing(), 90);
}

TEST_F(CameraTest, MeterFillsDecaysAndCameraCallInRaisesAlarmOnceAfterTwoSeconds) {
    int started = 0, triggered = 0;
    events.subscribe<CallInStarted>([&](const auto& event) {
        EXPECT_EQ(event.sourceId, "C01");
        EXPECT_EQ(event.sourceType, CallInSource::Camera);
        EXPECT_FLOAT_EQ(event.seconds, config.camera.callin);
        ++started;
    });
    events.subscribe<AlarmTriggered>([&](const auto&) { ++triggered; });
    updateCamera(0.5f);
    EXPECT_FLOAT_EQ(world->cameras.front().detection(), 50);
    world->player.pos = {24, 72};
    updateCamera(1);
    EXPECT_FLOAT_EQ(world->cameras.front().detection(), 25);
    world->player.pos = world->cameras.front().pos;
    updateCamera(0.75f);
    events.dispatch();
    EXPECT_EQ(started, 1);
    EXPECT_FLOAT_EQ(world->cameras.front().callInRemaining(), 2);
    world->player.pos = {500, 400};
    updateCamera(1.9f);
    alarm->update();
    EXPECT_EQ(alarm->state(), AlarmState::CallIn);
    updateCamera(0.11f);
    alarm->update();
    events.dispatch();
    EXPECT_EQ(alarm->state(), AlarmState::Loud);
    EXPECT_TRUE(world->alarmLoud);
    alarm->update();
    events.dispatch();
    EXPECT_EQ(triggered, 1);
}

TEST_F(CameraTest, LoopWinsExpiryTieCancelsOnlyCameraAndExpiresOnce) {
    int cancelled = 0, ended = 0;
    events.subscribe<CallInCancelled>([&](const auto& event) {
        EXPECT_EQ(event.sourceId, "C01");
        EXPECT_EQ(event.sourceType, CallInSource::Camera);
        EXPECT_EQ(event.reason, CallInCancelReason::Loop);
        ++cancelled;
    });
    events.subscribe<SecurityLoopEnded>([&](const auto&) { ++ended; });
    interaction.update(5.9f, true, *world);
    detection->update(1, world->player, world->level.map, world->guards);
    updateCamera(1);
    events.dispatch();
    updateCamera(1.9f);
    ASSERT_NEAR(world->cameras.front().callInRemaining(), 0.1f, 0.001f);
    interaction.update(0.11f, true, *world);
    ASSERT_GT(world->securityLoopRemaining, 0);
    updateCamera(0.11f, world->securityLoopRemaining > 0);
    detection->update(0.11f, world->player, world->level.map, world->guards);
    alarm->update();
    events.dispatch();
    events.dispatch();
    EXPECT_EQ(cancelled, 1);
    EXPECT_EQ(alarm->state(), AlarmState::CallIn);
    EXPECT_EQ(world->guards.front().state(), GuardState::Alerted);
    EXPECT_NEAR(world->guards.front().callInRemaining(), 2.89f, 0.001f);
    EXPECT_FALSE(world->cameras.front().callingIn());
    EXPECT_FLOAT_EQ(world->cameras.front().detection(), 0);
    updateCamera(100, true);
    EXPECT_FLOAT_EQ(world->cameras.front().detection(), 0);
    interaction.update(120, false, *world);
    interaction.update(1, false, *world);
    events.dispatch();
    EXPECT_EQ(ended, 1);
    EXPECT_FLOAT_EQ(world->securityLoopRemaining, 0);
    EXPECT_FALSE(interaction.targetAvailable(*world));
    updateCamera(0.1f);
    EXPECT_GT(world->cameras.front().detection(), 0);
}

TEST_F(CameraTest, LoudAlarmMakesUnusedPanelUnavailable) {
    alarm->trigger(AlarmReason::Thermite);
    interaction.update(10, true, *world);
    EXPECT_FALSE(world->securityLoopUsed);
    EXPECT_FLOAT_EQ(world->securityLoopRemaining, 0);
}

TEST_F(CameraTest, ClosedDoorOcclusionAndRangePreventFillAndLargeTickPreservesTimerRemainder) {
    std::istringstream source(".....\n..S..\n.....\n");
    world->level.map = TileMap::parse(source, 48);
    world->player.pos = {168, 72};
    updateCamera(1);
    EXPECT_FLOAT_EQ(world->cameras.front().detection(), 0);
    world->level.map.setOpen(2, 1, true);
    world->player.pos = {500, 72};
    updateCamera(1);
    EXPECT_FLOAT_EQ(world->cameras.front().detection(), 0);
    world->player.pos = world->cameras.front().pos;
    updateCamera(2);
    EXPECT_FLOAT_EQ(world->cameras.front().callInRemaining(), 1);
}
