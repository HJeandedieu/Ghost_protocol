#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "entities/Laser.h"
#include "entities/Player.h"
#include "systems/LaserSystem.h"

class LaserSystemTest : public testing::Test {
   protected:
    TileMap map;
    EventBus events;
    Player player{{240, 120}, PlayerConfig{}};
    std::vector<Laser> lasers;
    int noise = 0;
    std::vector<int> touches;
    void SetUp() override {
        std::istringstream source(
            "..........\n..........\n..........\n..........\n..........\n..........\n");
        map = TileMap::parse(source, 48);
        lasers.emplace_back(LaserSpawn{"L01", {1, 2}, {7, 2}}, map);
        lasers.emplace_back(LaserSpawn{"L02", {1, 4}, {7, 4}}, map);
        events.subscribe<NoiseEmitted>([&](const auto& event) {
            EXPECT_EQ(event.type, NoiseType::Laser);
            EXPECT_FLOAT_EQ(event.radius, 400);
            ++noise;
        });
        events.subscribe<LaserTouched>([&](const auto& event) { touches.push_back(event.count); });
    }
};

TEST_F(LaserSystemTest, FirstContactMakesNoiseAndCooldownLimitsContinuousContacts) {
    LaserSystem system(events, LaserConfig{}, 400);
    system.update(0.01f, player, lasers, false);
    system.update(1.49f, player, lasers, false);
    events.dispatch();
    ASSERT_EQ(touches, std::vector<int>{1});
    system.update(0.02f, player, lasers, false);
    events.dispatch();
    EXPECT_EQ(touches, (std::vector<int>{1, 2}));
    EXPECT_EQ(noise, 1);
}

TEST_F(LaserSystemTest, DifferentBeamsShareThirtySecondWindowAndExpiredWindowStartsFresh) {
    LaserSystem system(events, LaserConfig{}, 400);
    system.update(0.01f, player, lasers, false);
    player.pos = player.prevPos = {240, 216};
    system.update(30, player, lasers, false);
    events.dispatch();
    EXPECT_EQ(touches, (std::vector<int>{1, 2}));
    system.update(30.01f, player, lasers, false);
    events.dispatch();
    EXPECT_EQ(touches, (std::vector<int>{1, 2, 1}));
    EXPECT_EQ(noise, 2);
}

TEST_F(LaserSystemTest, LoopIgnoresTouchesAndResetsCounterWithoutHidingGeometry) {
    LaserSystem system(events, LaserConfig{}, 400);
    system.update(0.01f, player, lasers, false);
    events.dispatch();
    system.update(120, player, lasers, true);
    events.dispatch();
    EXPECT_EQ(touches.size(), 1u);
    EXPECT_EQ(system.count(), 0);
    system.update(0.01f, player, lasers, false);
    events.dispatch();
    EXPECT_EQ(touches, (std::vector<int>{1, 1}));
}

TEST_F(LaserSystemTest, FastCrossingDetectedButOutsideSegmentMisses) {
    LaserSystem system(events, LaserConfig{}, 400);
    player.prevPos = {240, 80};
    player.pos = {240, 160};
    system.update(1, player, lasers, false);
    events.dispatch();
    EXPECT_EQ(touches.size(), 1u);
    player.prevPos = {450, 80};
    player.pos = {450, 160};
    system.update(2, player, lasers, false);
    events.dispatch();
    EXPECT_EQ(touches.size(), 1u);
}

TEST_F(LaserSystemTest, SweptPlayerRadiusCanTouchBeamEndpoint) {
    LaserSystem system(events, LaserConfig{}, 400);
    player.prevPos = {60, 80};
    player.pos = {60, 160};
    system.update(1, player, lasers, false);
    events.dispatch();
    EXPECT_EQ(touches, std::vector<int>{1});
}
