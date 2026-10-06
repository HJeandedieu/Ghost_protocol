#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/GuardAI.h"
#include "entities/Player.h"
#include "systems/AlarmDirector.h"
#include "systems/DetectionSystem.h"
#include "world/World.h"

class AlarmDirectorTest : public testing::Test {
   protected:
    EventBus events;
    std::ostringstream console;
    Logger logger{console, ""};
    TileMap map;
    std::vector<Guard> guards;
    Player player{{72, 72}, PlayerConfig{}};
    int alarms = 0;
    AlarmReason reason = AlarmReason::CallIn;
    void SetUp() override {
        std::istringstream source("..........\n..........\n..........\n");
        map = TileMap::parse(source, 48);
        GuardSpawn spawn;
        spawn.id = "G01";
        spawn.mode = PatrolMode::Stationary;
        spawn.waypoints = {{1, 1}};
        guards.emplace_back(spawn, map, GuardConfig{});
        events.subscribe<AlarmTriggered>([this](const auto& event) {
            ++alarms;
            reason = event.reason;
        });
    }
};

TEST_F(AlarmDirectorTest, CallInExpiresOnceAndLoudIsPermanent) {
    DetectionSystem detection(events, logger, guards);
    AlarmDirector alarm(events, logger, guards);
    detection.update(1, player, map, guards);
    events.dispatch();
    EXPECT_EQ(alarm.state(), AlarmState::CallIn);
    EXPECT_FLOAT_EQ(alarm.callInRemaining(), 3);
    detection.update(2.9f, player, map, guards);
    alarm.update();
    EXPECT_EQ(alarm.state(), AlarmState::CallIn);
    detection.update(0.11f, player, map, guards);
    alarm.update();
    events.dispatch();
    EXPECT_EQ(alarm.state(), AlarmState::Loud);
    EXPECT_EQ(reason, AlarmReason::CallIn);
    alarm.trigger(AlarmReason::Pager);
    alarm.update();
    events.dispatch();
    EXPECT_EQ(alarms, 1);
}

TEST_F(AlarmDirectorTest, TakedownCancelsCallInAndUnconsciousBodiesNeverShout) {
    DetectionSystem detection(events, logger, guards);
    AlarmDirector alarm(events, logger, guards);
    detection.update(1, player, map, guards);
    events.dispatch();
    guards.front().takeDown(events);
    events.dispatch();
    EXPECT_EQ(alarm.state(), AlarmState::Quiet);
    detection.update(10, player, map, guards);
    alarm.update();
    events.dispatch();
    EXPECT_EQ(alarms, 0);
}

TEST_F(AlarmDirectorTest, SecondLaserTouchAndMissedPagerTriggerThroughEvents) {
    AlarmDirector alarm(events, logger, guards);
    events.publish(LaserTouched{"L01", 1});
    events.dispatch();
    EXPECT_EQ(alarm.state(), AlarmState::Quiet);
    events.publish(LaserTouched{"L01", 2});
    events.dispatch();
    events.dispatch();
    EXPECT_EQ(reason, AlarmReason::Laser);
    EXPECT_EQ(alarms, 1);
    events.publish(PagerMissed{"G01"});
    events.dispatch();
    events.dispatch();
    EXPECT_EQ(alarms, 1);
}

TEST_F(AlarmDirectorTest, MissedPagerAlarmsExactlyOnce) {
    AlarmDirector alarm(events, logger, guards);
    events.publish(PagerMissed{"G01"});
    events.publish(PagerMissed{"G01"});
    events.dispatch();
    events.dispatch();
    EXPECT_EQ(alarms, 1);
    EXPECT_EQ(reason, AlarmReason::Pager);
}

TEST_F(AlarmDirectorTest, UnsuppressedShotNeedsAnAwakeGuardInHearingRange) {
    AlarmDirector alarm(events, logger, guards);
    events.publish(NoiseEmitted{{72, 72}, 350, NoiseType::ShotSupp, "Ghost"});
    events.publish(NoiseEmitted{{400, 72}, 100, NoiseType::Shot, "Ghost"});
    events.dispatch();
    EXPECT_EQ(alarm.state(), AlarmState::Quiet);
    events.publish(NoiseEmitted{{400, 72}, 328, NoiseType::Shot, "Ghost"});
    events.dispatch();
    events.dispatch();
    EXPECT_EQ(reason, AlarmReason::Shot);
    EXPECT_EQ(alarms, 1);
}

TEST_F(AlarmDirectorTest, ThermiteTriggerTransitionsAwakeGuardsToCombat) {
    GuardAI ai(guards.front(), map, events, logger);
    AlarmDirector alarm(events, logger, guards);
    alarm.trigger(AlarmReason::Thermite);
    events.dispatch();
    EXPECT_EQ(reason, AlarmReason::Thermite);
    EXPECT_EQ(guards.front().state(), GuardState::Combat);
    EXPECT_FALSE(guards.front().takeDown(events));
    alarm.update();
    events.dispatch();
    EXPECT_EQ(alarms, 1);
}

TEST_F(AlarmDirectorTest, CombatShoutIsAnIndependentTrigger) {
    GuardAI ai(guards.front(), map, events, logger);
    events.publish(AlarmTriggered{AlarmReason::Combat});
    events.dispatch();
    alarms = 0;
    AlarmDirector alarm(events, logger, guards);
    alarm.update();
    events.dispatch();
    EXPECT_EQ(reason, AlarmReason::Combat);
    EXPECT_EQ(alarms, 1);
}

TEST(AlarmDirector, LoudLightsEveryTileAndOpensOnlyFrontEntriesOnce) {
    Level level;
    std::istringstream source("#####\n#@FS#\n#####\n");
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {1, 1};
    World world(std::move(level), PlayerConfig{});
    EventBus bus;
    std::ostringstream output;
    Logger logger(output, "");
    AlarmDirector alarm(bus, logger, world);
    EXPECT_FALSE(world.level.map.isOpen(2, 1));
    EXPECT_EQ(world.level.map.light(1, 1), LightLevel::Dark);
    alarm.trigger(AlarmReason::Pager);
    EXPECT_TRUE(world.alarmLoud);
    EXPECT_TRUE(world.level.map.isOpen(2, 1));
    EXPECT_FALSE(world.level.map.isOpen(3, 1));
    for (int y = 0; y < world.level.map.height(); ++y)
        for (int x = 0; x < world.level.map.width(); ++x)
            EXPECT_EQ(world.level.map.light(x, y), LightLevel::Lit);
    world.level.map.setLight(1, 1, LightLevel::Dim);
    alarm.trigger(AlarmReason::Shot);
    EXPECT_EQ(world.level.map.light(1, 1), LightLevel::Dim);
}
