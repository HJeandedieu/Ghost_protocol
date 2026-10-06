#include <gtest/gtest.h>

#include <memory>
#include <sstream>

#include "core/EventBus.h"
#include "systems/InteractionSystem.h"
#include "systems/PagerSystem.h"
#include "world/World.h"

class PagerSystemTest : public testing::Test {
   protected:
    EventBus events;
    InteractionSystem interaction{events};
    std::unique_ptr<World> world;
    std::unique_ptr<PagerSystem> pagers;
    int rings = 0, answered = 0, missed = 0;
    void SetUp() override {
        Level level;
        std::istringstream source(".....\n.....\n.....\n");
        level.map = TileMap::parse(source, 48);
        level.playerSpawn = {1, 1};
        GuardSpawn spawn;
        spawn.id = "G01";
        spawn.mode = PatrolMode::Stationary;
        spawn.waypoints = {{1, 1}};
        spawn.pager = true;
        level.guards.push_back(spawn);
        spawn.id = "G02";
        spawn.pager = false;
        spawn.waypoints = {{3, 1}};
        level.guards.push_back(spawn);
        world = std::make_unique<World>(std::move(level), PlayerConfig{});
        pagers = std::make_unique<PagerSystem>(events, PagerConfig{}, *world, interaction);
        events.subscribe<PagerRang>([&](const auto&) { ++rings; });
        events.subscribe<PagerAnswered>([&](const auto&) { ++answered; });
        events.subscribe<PagerMissed>([&](const auto&) { ++missed; });
    }
    void takeDown() {
        world->guards[0].takeDown(events);
        events.dispatch();
    }
};

TEST_F(PagerSystemTest, RingsAtFourSecondsAndMissesAtEndOfTwelveSecondWindowOnce) {
    takeDown();
    pagers->update(3.9f);
    EXPECT_EQ(pagers->state("G01"), PagerState::Waiting);
    pagers->update(0.11f);
    events.dispatch();
    EXPECT_EQ(rings, 1);
    EXPECT_NEAR(pagers->remaining("G01"), 11.99f, 0.001f);
    pagers->update(12);
    pagers->update(100);
    events.dispatch();
    EXPECT_EQ(pagers->state("G01"), PagerState::Missed);
    EXPECT_EQ(missed, 1);
}

TEST_F(PagerSystemTest, AnswerRequiresFullHoldAndReleasingOrLeavingResetsIt) {
    takeDown();
    interaction.update(2, true, *world);
    EXPECT_EQ(pagers->state("G01"), PagerState::Waiting);
    pagers->update(4);
    interaction.update(1, true, *world);
    interaction.update(0.01f, false, *world);
    interaction.update(1, true, *world);
    EXPECT_EQ(pagers->state("G01"), PagerState::Ringing);
    world->player.pos = {200, 72};
    interaction.update(0.01f, true, *world);
    world->player.pos = {72, 72};
    interaction.update(1.4f, true, *world);
    EXPECT_EQ(pagers->state("G01"), PagerState::Ringing);
    interaction.update(0.11f, true, *world);
    events.dispatch();
    EXPECT_EQ(answered, 1);
    pagers->update(30);
    events.dispatch();
    EXPECT_EQ(missed, 0);
}

TEST_F(PagerSystemTest, NonPagerGuardDoesNotRingAndLongTickPreservesRingBeforeMiss) {
    world->guards[1].takeDown(events);
    events.dispatch();
    pagers->update(30);
    events.dispatch();
    EXPECT_EQ(rings, 0);
    takeDown();
    pagers->update(16);
    events.dispatch();
    EXPECT_EQ(rings, 1);
    EXPECT_EQ(missed, 1);
}
