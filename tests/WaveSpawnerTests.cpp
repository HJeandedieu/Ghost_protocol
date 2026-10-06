#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/EventBus.h"
#include "core/Logger.h"
#include "systems/CombatSystem.h"
#include "systems/EnemyCombatSystem.h"
#include "systems/PickupSystem.h"
#include "systems/WaveSpawner.h"
#include "world/World.h"
namespace {
Level arena() {
    Level level;
    std::istringstream input(
        "############\n#..........#\n#..........#\n#..........#\n#..........#\n############\n");
    level.map = TileMap::parse(input, 64);
    level.playerSpawn = {2, 2};
    return level;
}
constexpr SpawnView kOffscreen{-1000, -1000, -900, -900};
}  // namespace
class WaveTest : public testing::Test {
   protected:
    std::ostringstream output;
    Logger logger{output, ""};
    EventBus bus;
    Config config;
    World world{arena(), config.player};
    std::vector<EnemySpec> enemies = loadEnemies("assets/config/enemies.json", logger).value();
    std::vector<WaveSpec> waves = loadWaves("assets/config/enemies.json", logger).value();
    std::map<std::string, TileCoord> entries{
        {"front", {2, 2}}, {"service", {6, 2}}, {"east", {9, 2}}};
    std::vector<WeaponSpec> guns = loadWeapons("assets/config/weapons.json", logger).value();
    CombatSystem combat{bus, guns, 42};
    void clearPolice() {
        for (auto& enemy : world.enemies) combat.applyDamage(*enemy, 1000, "Ghost");
        bus.dispatch();
    }
};
TEST_F(WaveTest, QuietClockDoesNotAdvanceAndFirstWaveStartsAtThirtySeconds) {
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    int notifications = 0;
    bus.subscribe<WaveSpawned>([&](const auto& event) {
        EXPECT_EQ(event.waveIndex, 1);
        ++notifications;
    });
    spawner.update(100, kOffscreen);
    EXPECT_EQ(spawner.waveIndex(), 0);
    world.alarmLoud = true;
    for (int i = 0; i < 1799; ++i) spawner.update(1.f / 60, kOffscreen);
    EXPECT_TRUE(world.enemies.empty());
    spawner.update(1.f / 60, kOffscreen);
    EXPECT_EQ(world.enemies.size(), 4u);
    EXPECT_EQ(spawner.waveIndex(), 1);
    EXPECT_EQ(notifications, 0);
    bus.dispatch();
    EXPECT_EQ(notifications, 1);
    EXPECT_NEAR(spawner.nextWaveRemaining(), 25, 0.001);
    EXPECT_FLOAT_EQ(world.enemies[0]->pos.x, 160);
    EXPECT_FLOAT_EQ(world.enemies[1]->pos.x, 416);
}
TEST_F(WaveTest, EveryCompositionAndRepeatMatchesCatalogAfterDeaths) {
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    world.alarmLoud = true;
    std::vector<int> indices;
    bus.subscribe<WaveSpawned>([&](const auto& event) { indices.push_back(event.waveIndex); });
    for (int wave = 0; wave < 7; ++wave) {
        clearPolice();
        spawner.update(wave == 0 ? 30.f : 25.f, kOffscreen);
        bus.dispatch();
        std::array<int, 3> counts{};
        for (const auto& enemy : world.enemies)
            if (!enemy->dead()) {
                if (enemy->spec().id == "cop") ++counts[0];
                if (enemy->spec().id == "shield_cop") {
                    ++counts[1];
                    EXPECT_EQ(enemy->radius, 16);
                }
                if (enemy->spec().id == "heavy") {
                    ++counts[2];
                    EXPECT_EQ(enemy->hp(), 250);
                    EXPECT_EQ(enemy->armor(), 100);
                    EXPECT_EQ(enemy->radius, 20);
                }
            }
        EXPECT_EQ(counts, waves[wave < 5 ? wave : 5].counts);
    }
    EXPECT_EQ(indices, (std::vector<int>{1, 2, 3, 4, 5, 6, 7}));
}
TEST_F(WaveTest, AliveCapDiscardsExcessAndLaterDeathsDoNotRestoreOldWave) {
    config.alarm.maxAlive = 3;
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    world.alarmLoud = true;
    spawner.update(30, kOffscreen);
    EXPECT_EQ(world.enemies.size(), 3u);
    EXPECT_EQ(spawner.pendingCount(), 0u);
    clearPolice();
    spawner.update(1, kOffscreen);
    EXPECT_EQ(world.enemies.size(), 3u);
    spawner.update(24, kOffscreen);
    EXPECT_EQ(world.enemies.size(), 6u);
}
TEST_F(WaveTest, OverflowAcceptsCopsBeforeShieldsBeforeHeaviesAndGuardsDoNotCount) {
    config.alarm.maxAlive = 5;
    waves[0].counts = {4, 2, 1};
    GuardSpawn guard;
    guard.id = "guard";
    guard.waypoints = {{4, 2}};
    world.guards.emplace_back(guard, world.level.map, config.guard);
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    world.alarmLoud = true;
    spawner.update(30, kOffscreen);
    ASSERT_EQ(world.enemies.size(), 5u);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(world.enemies[i]->spec().id, "cop");
    EXPECT_EQ(world.enemies[4]->spec().id, "shield_cop");
}
TEST_F(WaveTest, ConfiguredHardCapAllowsSixteenPolice) {
    config.alarm.maxAlive = config.difficulty.hard.maxAlive;
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    world.alarmLoud = true;
    spawner.update(105, kOffscreen);
    EXPECT_EQ(world.enemies.size(), 16u);
    EXPECT_EQ(spawner.waveIndex(), 4);
}
TEST_F(WaveTest, PendingVisibleEntriesReserveCapacityAndReleaseOnlyAcceptedEnemies) {
    config.alarm.maxAlive = 5;
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    world.alarmLoud = true;
    const SpawnView visible{0, 0, 768, 384};
    spawner.update(30, visible);
    EXPECT_EQ(spawner.pendingCount(), 4u);
    EXPECT_TRUE(world.enemies.empty());
    spawner.update(25, visible);
    EXPECT_EQ(spawner.pendingCount(), 5u);
    EXPECT_EQ(spawner.waveIndex(), 2);
    spawner.update(1, kOffscreen);
    EXPECT_EQ(world.enemies.size(), 5u);
    EXPECT_EQ(spawner.pendingCount(), 0u);
    clearPolice();
    spawner.update(1, kOffscreen);
    EXPECT_EQ(world.enemies.size(), 5u);
}
TEST_F(WaveTest, BlockedEntryWaitsAndRotationSkipsVisibleEntry) {
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    world.alarmLoud = true;
    std::istringstream blocked(
        "############\n#..........#\n#.#...#....#\n#..........#\n#..........#\n############\n");
    world.level.map = TileMap::parse(blocked, 64);
    spawner.update(30, kOffscreen);
    EXPECT_EQ(spawner.pendingCount(), 4u);
    world.level.map = arena().map;
    spawner.update(1, {140, 140, 180, 180});
    ASSERT_EQ(world.enemies.size(), 4u);
    for (const auto& enemy : world.enemies) EXPECT_FLOAT_EQ(enemy->pos.x, 416);
}
TEST_F(WaveTest, EntireCircleMustBeOutsideViewAndInvalidDeltaDoesNotAdvance) {
    WaveSpawner spawner(bus, world, enemies, waves, entries, config.alarm);
    world.alarmLoud = true;
    spawner.update(-1, kOffscreen);
    spawner.update(std::numeric_limits<float>::quiet_NaN(), kOffscreen);
    EXPECT_EQ(spawner.waveIndex(), 0);
    spawner.update(30, {173, 0, 768, 384});
    EXPECT_EQ(spawner.pendingCount(), 4u);
    spawner.update(1, {175, 0, 768, 384});
    EXPECT_EQ(spawner.pendingCount(), 0u);
}
TEST_F(WaveTest, ShieldFrontalEdgesBlockAndSidesRearDoNot) {
    ShieldCop shield("S01", {300, 160}, enemies[2]);
    constexpr float pi = 3.14159265358979323846f;
    for (float angle : {-60.f, 0.f, 60.f}) {
        const Vec2 from{shield.pos.x + 100 * std::cos(angle * pi / 180),
                        shield.pos.y + 100 * std::sin(angle * pi / 180)};
        EXPECT_NEAR(shield.hitscanDamage(100, from), 10, 0.001);
    }
    for (float angle : {-61.f, 61.f, 90.f, 180.f}) {
        const Vec2 from{shield.pos.x + 100 * std::cos(angle * pi / 180),
                        shield.pos.y + 100 * std::sin(angle * pi / 180)};
        EXPECT_EQ(shield.hitscanDamage(100, from), 100);
    }
}
TEST_F(WaveTest, PlayerPelletsApplyShieldReductionAndRearKillsDropOnce) {
    config.pickup.medkitChance = 1;
    config.pickup.armorChance = 0;
    PickupSystem pickups(bus, config.pickup, 42);
    EnemyCombatSystem ai(bus, world, combat, pickups, enemies, config, 42);
    world.enemies.push_back(std::make_unique<ShieldCop>("S01", Vec2{300, 160}, enemies[2]));
    auto gun = guns[0];
    gun.damage = 100;
    gun.pellets = 2;
    gun.spreadDeg = 0;
    gun.range = 600;
    Rng rng(42);
    Weapon weapon(gun);
    auto front = combat.fire(weapon, {400, 160}, 180, rng, world);
    EXPECT_NEAR(world.enemies[0]->hp(), 100, 0.001);
    EXPECT_NEAR(front.pellets[0].damage, 10, 0.001);
    combat.fire(weapon, {160, 160}, 0, rng, world);
    bus.dispatch();
    EXPECT_TRUE(world.enemies[0]->dead());
    EXPECT_EQ(world.pickups.size(), 1u);
    combat.applyDamage(*world.enemies[0], 100, "Ghost");
    bus.dispatch();
    EXPECT_EQ(world.pickups.size(), 1u);
    ai.update(0.6f);
    EXPECT_NEAR(world.enemies[0]->deathOpacity(), 0, 0.001);
}
TEST_F(WaveTest, HeavyHoldsEngageAndShieldContinuesAdvancingWithSeededShots) {
    PickupSystem pickups(bus, config.pickup, 42);
    enemies[2].accuracy = enemies[3].accuracy = 0;
    EnemyCombatSystem ai(bus, world, combat, pickups, enemies, config, 42);
    world.enemies.push_back(std::make_unique<Heavy>("H01", Vec2{300, 160}, enemies[3]));
    world.enemies.push_back(std::make_unique<ShieldCop>("S01", Vec2{300, 200}, enemies[2]));
    int heavyShots = 0;
    bus.subscribe<ShotFired>([&](const auto& event) {
        if (event.shooterId == "H01") ++heavyShots;
    });
    for (int i = 0; i < 60; ++i) {
        ai.update(1.f / 60);
        bus.dispatch();
    }
    EXPECT_FLOAT_EQ(world.enemies[0]->pos.x, 300);
    EXPECT_FLOAT_EQ(world.enemies[0]->pos.y, 160);
    EXPECT_EQ(heavyShots, 5);
    EXPECT_LT(world.enemies[1]->pos.x, 300);
    combat.applyDamage(*world.enemies[0], 125, "Ghost");
    EXPECT_FLOAT_EQ(world.enemies[0]->armor(), 0);
    EXPECT_FLOAT_EQ(world.enemies[0]->hp(), 225);
}
TEST(WaveData, LoadsCatalogAndRejectsInvalidCountsTypesIndicesAndPoints) {
    std::ostringstream output;
    Logger logger(output, "");
    TestFiles files;
    nlohmann::json original;
    std::ifstream("assets/config/enemies.json") >> original;
    const auto waves = loadWaves("assets/config/enemies.json", logger);
    ASSERT_TRUE(waves);
    EXPECT_EQ(waves->size(), 6u);
    EXPECT_EQ(waves->back().index, -1);
    for (const auto& count : {nlohmann::json(0), nlohmann::json(-1), nlohmann::json(1.5),
                              nlohmann::json(4294967296LL)}) {
        auto data = original;
        data["waves"][0]["spawn"]["cop"] = count;
        EXPECT_FALSE(loadWaves(files.write("bad-count.json", data.dump()), logger));
    }
    auto data = original;
    data["waves"][0]["spawn"] = {{"patrol_guard", 1}};
    EXPECT_FALSE(loadWaves(files.write("bad-type.json", data.dump()), logger));
    data = original;
    data["waves"][1]["at"] = 0;
    EXPECT_FALSE(loadWaves(files.write("duplicate.json", data.dump()), logger));
    data = original;
    data["waves"][0]["points"] = {"unknown"};
    EXPECT_FALSE(loadWaves(files.write("bad-point.json", data.dump()), logger));
    data = original;
    data["waves"][0]["points"] = nlohmann::json::array();
    EXPECT_FALSE(loadWaves(files.write("empty-points.json", data.dump()), logger));
    EXPECT_FALSE(loadWaves(files.write("malformed.json", "{"), logger));
    EXPECT_FALSE(loadWaves(files.path("missing.json"), logger));
}
TEST(Config, ZeroWaveIntervalFallsBackInsteadOfCreatingAnInfiniteSchedule) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    data["alarm"]["wave_interval"] = 0;
    std::ostringstream output;
    Logger logger(output, "");
    const auto config = Config::load(files.write("zero-interval.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(config.alarm.waveInterval, 25);
    EXPECT_NE(output.str().find("[WARN]"), std::string::npos);
}
