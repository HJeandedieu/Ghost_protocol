#include <gtest/gtest.h>

#include <limits>

#include "core/EventBus.h"
#include "systems/NoiseSystem.h"

TEST(NoiseSystem, HearingUsesCurrentPatrolPositionInsteadOfSpawn) {
    EventBus bus;
    NoiseSystem noise(bus);
    noise.setHearers({{"G01", {400, 0}}});
    int heard = 0;
    bus.subscribe<GuardSuspicious>([&](const GuardSuspicious&) { ++heard; });
    noise.emit({0, 0}, 120, NoiseType::Step, "Ghost");
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(heard, 0);
    noise.setHearerPosition(0, {100, 0});
    noise.emit({0, 0}, 120, NoiseType::Step, "Ghost");
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(heard, 1);
}

TEST(NoiseSystem, NotifiesOnlyHearersInsideRadiusIncludingBoundaryRegardlessOfWalls) {
    EventBus bus;
    NoiseSystem noise(bus);
    noise.setHearers(
        {{"near", {30, 40}}, {"edge", {100, 0}}, {"far", {101, 0}}, {"Ghost", {0, 0}}});
    std::vector<std::string> heard;
    bus.subscribe<GuardSuspicious>([&](const GuardSuspicious& event) {
        heard.push_back(event.guardId);
        EXPECT_FLOAT_EQ(event.point.x, 0);
    });
    noise.emit({0, 0}, 100, NoiseType::Ping, "Ghost");
    bus.dispatch();
    EXPECT_TRUE(heard.empty());
    EXPECT_FLOAT_EQ(noise.currentRadius(), 100);
    bus.dispatch();
    ASSERT_EQ(heard.size(), 2u);
    EXPECT_EQ(heard[0], "near");
    EXPECT_EQ(heard[1], "edge");
    // Hearing has no tile-map/line-of-sight dependency; geometry cannot block it.
}

TEST(NoiseSystem, PreservesTypeRadiusAndSourceAndResetsHudStateForNextTick) {
    EventBus bus;
    NoiseSystem noise(bus);
    int count = 0;
    bus.subscribe<NoiseEmitted>([&](const NoiseEmitted& e) {
        ++count;
        EXPECT_EQ(e.type, NoiseType::Lockpick);
        EXPECT_EQ(e.sourceId, "Ghost");
        EXPECT_FLOAT_EQ(e.radius, 120);
    });
    noise.emit({1, 2}, 120, NoiseType::Lockpick, "Ghost");
    bus.dispatch();
    EXPECT_EQ(count, 1);
    EXPECT_FLOAT_EQ(noise.currentRadius(), 120);
    noise.beginTick();
    EXPECT_FLOAT_EQ(noise.currentRadius(), 0);
}

TEST(NoiseSystem, InvalidNoiseAndDistantHearersProduceNoNotifications) {
    EventBus bus;
    NoiseSystem noise(bus);
    noise.setHearers({{"guard", {1000, 1000}}});
    int count = 0;
    bus.subscribe<GuardSuspicious>([&](const GuardSuspicious&) { ++count; });
    noise.emit({}, -1, NoiseType::Step, "Ghost");
    noise.emit({}, std::numeric_limits<float>::quiet_NaN(), NoiseType::Step, "Ghost");
    noise.emit({}, 40, NoiseType::Step, "Ghost");
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(count, 0);
    EXPECT_FLOAT_EQ(noise.currentRadius(), 40);
}
