#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <queue>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "systems/InteractionSystem.h"
#include "world/LevelLoader.h"
#include "world/Raycast.h"
#include "world/World.h"

namespace {
World sampleWorld(const std::string& text) {
    std::istringstream source(text);
    Level level;
    level.name = "Test Bank";
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {1, 1};
    return World(std::move(level), PlayerConfig{});
}
void hold(InteractionSystem& interaction, World& world, EventBus& bus, float seconds) {
    const int ticks = static_cast<int>(std::ceil(seconds * 60));
    for (int i = 0; i < ticks; ++i) {
        interaction.update(1.0f / 60, true, world);
        bus.dispatch();
    }
}
TileCoord locate(const TileMap& map, TileType type) {
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
            if (map.tile(x, y) == type) return {x, y};
    return {-1, -1};
}
bool walkTo(World& world, InteractionSystem& interaction, TileCoord destination) {
    const auto& map = world.level.map;
    const int width = map.width();
    const auto index = [width](TileCoord p) { return p.y * width + p.x; };
    const TileCoord start{static_cast<int>(world.player.pos.x / map.tileSize()),
                          static_cast<int>(world.player.pos.y / map.tileSize())};
    std::vector<int> previous(static_cast<std::size_t>(width * map.height()), -1);
    std::queue<TileCoord> frontier;
    frontier.push(start);
    previous[index(start)] = index(start);
    while (!frontier.empty()) {
        const auto p = frontier.front();
        frontier.pop();
        for (const auto offset :
             {TileCoord{1, 0}, TileCoord{-1, 0}, TileCoord{0, 1}, TileCoord{0, -1}}) {
            const TileCoord next{p.x + offset.x, p.y + offset.y};
            if (!map.isPassable(next.x, next.y)) continue;
            if (previous[index(next)] >= 0) continue;
            previous[index(next)] = index(p);
            frontier.push(next);
        }
    }
    if (previous[index(destination)] < 0) return false;
    std::vector<int> route;
    for (int p = index(destination); p != index(start); p = previous[p]) route.push_back(p);
    std::reverse(route.begin(), route.end());
    for (const int p : route) {
        const auto center = map.tileCenter({p % width, p / width});
        bool reached = false;
        for (int tick = 0; tick < 600; ++tick) {
            const Vec2 delta{center.x - world.player.pos.x, center.y - world.player.pos.y};
            if (std::hypot(delta.x, delta.y) < 2) {
                reached = true;
                break;
            }
            Input input;
            input.move = delta;
            world.player.update(1.0f / 60, input, map);
            interaction.update(1.0f / 60, false, world);
        }
        if (!reached) return false;
    }
    return true;
}
}  // namespace

TEST(InteractionSystem, HoldCompletesExactlyOnceAndReleaseOrRangeExitResetsProgress) {
    auto world = sampleWorld(".....\n.....\n.....");
    EventBus bus;
    InteractionSystem interaction(bus);
    bool completed = false;
    int count = 0;
    interaction.add({"test",
                     {72, 72},
                     1,
                     "Hold E",
                     [&](const Player&) { return !completed; },
                     [&](World&) { completed = true; }});
    bus.subscribe<InteractionDone>([&](const InteractionDone&) { ++count; });
    interaction.update(0.4f, true, world);
    EXPECT_FLOAT_EQ(interaction.progress(), 0.4f);
    interaction.update(0.1f, false, world);
    EXPECT_FLOAT_EQ(interaction.progress(), 0);
    interaction.update(0.4f, true, world);
    world.player.pos = {200, 72};
    interaction.update(0.1f, true, world);
    EXPECT_EQ(interaction.target(), nullptr);
    world.player.pos = {72, 72};
    hold(interaction, world, bus, 1.1f);
    EXPECT_TRUE(completed);
    EXPECT_EQ(count, 1);
    hold(interaction, world, bus, 1);
    EXPECT_EQ(count, 1);
}

TEST(InteractionSystem, QuietCrackRetainsProgressAndDecaysAtOneThirdRate) {
    auto world = sampleWorld(".....\n.....\n.....");
    EventBus bus;
    InteractionSystem interaction(bus);
    interaction.add({"vault",
                     {72, 72},
                     25,
                     "Crack vault",
                     [](const Player&) { return true; },
                     [](World&) {},
                     true});
    interaction.update(3, true, world);
    EXPECT_FLOAT_EQ(interaction.progress(), 3.0f / 25);
    interaction.update(3, false, world);
    EXPECT_FLOAT_EQ(interaction.progress(), 2.0f / 25);
}

TEST(InteractionSystem, WallPreventsInteractionEvenWithinRange) {
    auto world = sampleWorld(".....\n.@#..\n.....");
    EventBus bus;
    InteractionSystem interaction(bus);
    bool completed = false;
    world.player.pos = {90, 72};
    interaction.add({"blocked",
                     {130, 72},
                     0,
                     "E",
                     [](const Player&) { return true; },
                     [&](World&) { completed = true; }});
    interaction.update(1, true, world);
    EXPECT_FALSE(completed);
    EXPECT_EQ(interaction.target(), nullptr);
}

TEST(InteractionSystem, LockedCardDoorNeedsKeycardAndNormalDoorsOpenOnContact) {
    auto world = sampleWorld("......\n.@Rd..\n......");
    Config config;
    EventBus bus;
    InteractionSystem interaction(bus);
    interaction.loadBank(world, config);
    hold(interaction, world, bus, 1);
    EXPECT_FALSE(world.level.map.isOpen(2, 1));
    world.player.collectKeycard();
    hold(interaction, world, bus, 0.1f);
    EXPECT_TRUE(world.level.map.isOpen(2, 1));
    EXPECT_TRUE(world.level.map.isPassable(2, 1));
    EXPECT_FALSE(world.level.map.blocksSight(2, 1));
    world.player.pos = {3 * 48.0f - world.player.radius, 72};
    interaction.update(0.1f, false, world);
    EXPECT_TRUE(world.level.map.isOpen(3, 1));
}

TEST(InteractionSystem, SecurityLoopIsTimedAndSingleUse) {
    auto world = sampleWorld(".....\n.@P..\n.....");
    Config config;
    EventBus bus;
    InteractionSystem interaction(bus);
    interaction.loadBank(world, config);
    int loops = 0;
    bus.subscribe<SecurityLooped>([&](const SecurityLooped& e) {
        ++loops;
        EXPECT_FLOAT_EQ(e.secondsLeft, 120);
    });
    hold(interaction, world, bus, 6.1f);
    EXPECT_EQ(loops, 1);
    EXPECT_TRUE(world.securityLoopUsed);
    EXPECT_GT(world.securityLoopRemaining, 119);
    interaction.update(200, false, world);
    EXPECT_FLOAT_EQ(world.securityLoopRemaining, 0);
    hold(interaction, world, bus, 7);
    EXPECT_EQ(loops, 1);
}

TEST(InteractionSystem, ShippedBankServiceCardBreakerAndGateProgression) {
    std::ostringstream console;
    Logger logger(console, "");
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    const auto config = Config::load("assets/config/tuning.json", logger);
    World world(std::move(*level), config.player);
    EventBus bus;
    InteractionSystem interaction(bus);
    interaction.loadBank(world, config);
    const auto service = locate(world.level.map, TileType::ServiceDoor);
    ASSERT_TRUE(walkTo(world, interaction, {service.x - 1, service.y}));
    Input approach;
    approach.move = {1, 0};
    for (int i = 0; i < 6; ++i) world.player.update(1.0f / 60, approach, world.level.map);
    hold(interaction, world, bus, 3.9f);
    EXPECT_FALSE(world.level.map.isOpen(service.x, service.y));
    hold(interaction, world, bus, 0.2f);
    EXPECT_TRUE(world.level.map.isOpen(service.x, service.y));
    const auto card = locate(world.level.map, TileType::Keycard);
    ASSERT_TRUE(walkTo(world, interaction, card));
    hold(interaction, world, bus, 0.1f);
    EXPECT_TRUE(world.player.hasKeycard());
    EXPECT_EQ(world.level.map.tile(card.x, card.y), TileType::Floor);
    const auto red = locate(world.level.map, TileType::CardDoor);
    ASSERT_TRUE(walkTo(world, interaction, {red.x - 1, red.y}));
    for (int i = 0; i < 6; ++i) world.player.update(1.0f / 60, approach, world.level.map);
    hold(interaction, world, bus, 0.1f);
    EXPECT_TRUE(world.level.map.isOpen(red.x, red.y));
    const auto breaker = locate(world.level.map, TileType::Breaker);
    ASSERT_TRUE(walkTo(world, interaction, breaker));
    hold(interaction, world, bus, 5.1f);
    EXPECT_TRUE(world.powerOn);
    ASSERT_TRUE(walkTo(world, interaction, locate(world.level.map, TileType::Gate)));
    for (int y = 0; y < world.level.map.height(); ++y)
        for (int x = 0; x < world.level.map.width(); ++x) {
            if (world.level.map.tile(x, y) == TileType::Gate) {
                EXPECT_TRUE(world.level.map.isOpen(x, y));
            }
            if (world.level.map.tile(x, y) == TileType::VaultDoor) {
                EXPECT_FALSE(world.level.map.isOpen(x, y));
            }
        }
}

TEST(TileMap, DoorAndBollardStateChangesRespectCollisionAndSight) {
    std::istringstream source("#SRGVFb.");
    auto map = TileMap::parse(source, 48);
    for (int x = 1; x <= 6; ++x) {
        EXPECT_FALSE(map.isPassable(x, 0));
        map.setOpen(x, 0, true);
        EXPECT_TRUE(map.isPassable(x, 0));
        EXPECT_FALSE(map.blocksSight(x, 0));
        map.setOpen(x, 0, false);
        EXPECT_FALSE(map.isPassable(x, 0));
    }
    EXPECT_THROW(map.setOpen(0, 0, true), std::invalid_argument);
    EXPECT_THROW(map.setOpen(-1, 0, true), std::out_of_range);
}
