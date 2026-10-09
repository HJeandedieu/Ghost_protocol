#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/GuardAI.h"
#include "systems/AlarmDirector.h"
#include "systems/CombatSystem.h"
#include "systems/DetectionSystem.h"
#include "systems/EnemyCombatSystem.h"
#include "systems/PickupSystem.h"
#include "world/World.h"

namespace {
Level arena() {
    Level level;
    std::istringstream ascii(
        "############\n#..........#\n#..........#\n#..........#\n#..........#\n############\n");
    level.map = TileMap::parse(ascii, 64);
    level.playerSpawn = {2, 2};
    return level;
}
}  // namespace
class EnemyCombatTest : public testing::Test {
   protected:
    std::ostringstream output;
    Logger logger{output, ""};
    EventBus bus;
    Config config = [] {
        Config c;
        c.pickup.medkitChance = 1;
        c.pickup.armorChance = 0;
        return c;
    }();
    World world{arena(), config.player};
    std::vector<WeaponSpec> weapons = loadWeapons("assets/config/weapons.json", logger).value();
    std::vector<EnemySpec> specs = loadEnemies("assets/config/enemies.json", logger).value();
    CombatSystem combat{bus, weapons, 42};
    PickupSystem pickups{bus, config.pickup, 42};
    std::unique_ptr<EnemyCombatSystem> ai;
    int shots = 0;
    void start(float accuracy = 1) {
        config.enemyCombat.strafeSpeed = 0;
        for (auto& spec : specs)
            if (spec.id == "cop") spec.accuracy = accuracy;
        ai = std::make_unique<EnemyCombatSystem>(bus, world, combat, pickups, specs, config, 42);
        auto spec = specs[1];
        for (const auto& entry : specs)
            if (entry.id == "cop") spec = entry;
        world.enemies.push_back(std::make_unique<Cop>("C01", Vec2{300, 160}, spec, 14));
        bus.subscribe<ShotFired>([this](const auto& event) {
            if (event.shooterId == "C01") ++shots;
        });
    }
    void tick(int count) {
        for (int i = 0; i < count; ++i) {
            ai->update(1.f / 60);
            bus.dispatch();
        }
    }
};
TEST_F(EnemyCombatTest, SuccessfulRollUsesCrouchedBodyHeightAndRequiresThreeDimensionalRange) {
    start();
    config.shotGeometry.playerCrouchHeight = 8;
    config.view.crouchEyeHeight = 4;
    Input input;
    input.crouchPressed = true;
    world.player.update(1.f / 60, input, world.level.map);
    ASSERT_TRUE(world.player.isCrouched());
    auto spec = specs[1];
    spec.accuracy = 1;
    spec.engage = 12;
    spec.rate = 2;
    world.enemies.clear();
    world.enemies.push_back(std::make_unique<Cop>("C01", Vec2{170, 160}, spec, spec.radius));
    tick(24);
    ASSERT_EQ(ai->shots().size(), 1u);
    EXPECT_FALSE(ai->shots()[0].hitPlayer);
    EXPECT_FLOAT_EQ(world.player.armor(), 50);
    EXPECT_FLOAT_EQ(ai->shots()[0].from3D.y, 24);
    const auto& shot = ai->shots()[0];
    const auto delta = shot.to3D - shot.from3D;
    EXPECT_NEAR(std::sqrt(delta.dot(delta)), 12, .0001);
}

TEST_F(EnemyCombatTest, SuccessfulShotHitsCrouchedCylinderButFailedRollAlwaysMisses) {
    for (float accuracy : {1.f, 0.f}) {
        world.enemies.clear();
        start(accuracy);
        Input input;
        input.crouchPressed = !world.player.isCrouched();
        world.player.update(1.f / 60, input, world.level.map);
        const float armor = world.player.armor();
        tick(24);
        ASSERT_EQ(ai->shots().size(), 1u);
        const auto& shot = ai->shots()[0];
        EXPECT_EQ(shot.hitPlayer, accuracy == 1);
        EXPECT_FLOAT_EQ(shot.from3D.y, config.shotGeometry.copHeight * .5f);
        if (accuracy == 1) {
            EXPECT_LT(world.player.armor(), armor);
            EXPECT_GE(shot.to3D.y, 0);
            EXPECT_LE(shot.to3D.y, config.shotGeometry.playerCrouchHeight);
        } else
            EXPECT_FLOAT_EQ(world.player.armor(), armor);
    }
}

TEST_F(EnemyCombatTest, EnemyBulletsDoNotDamageOrStopAtOtherEnemies) {
    start();
    auto spec = specs[1];
    spec.damage = 0;
    spec.accuracy = 0;
    world.enemies.push_back(std::make_unique<Cop>("C02", Vec2{230, 160}, spec, spec.radius));
    tick(24);
    EXPECT_FLOAT_EQ(world.enemies[0]->hp(), 100);
    EXPECT_FLOAT_EQ(world.enemies[1]->hp(), 100);
    EXPECT_FLOAT_EQ(world.player.armor(), 43);
}

TEST_F(EnemyCombatTest, CoincidentBodyCentersDoNotProduceZeroLengthShotsOrDamage) {
    start();
    world.enemies[0]->pos = world.player.pos;
    tick(24);
    EXPECT_TRUE(ai->shots().empty());
    EXPECT_EQ(shots, 0);
    EXPECT_FLOAT_EQ(world.player.armor(), 50);
}
TEST_F(EnemyCombatTest, ReactionThenThreeBulletsAndBurstStartCadence) {
    start();
    tick(23);
    EXPECT_EQ(shots, 0);
    tick(1);
    EXPECT_EQ(shots, 1);
    EXPECT_FLOAT_EQ(world.player.armor(), 43);
    tick(7);
    EXPECT_EQ(shots, 1);
    tick(1);
    EXPECT_EQ(shots, 2);
    tick(6);
    EXPECT_EQ(shots, 2);
    tick(1);
    EXPECT_EQ(shots, 3);
    EXPECT_FLOAT_EQ(world.player.armor(), 29);
    tick(34);
    EXPECT_EQ(shots, 3);
    tick(1);
    EXPECT_EQ(shots, 4);
}
TEST_F(EnemyCombatTest, LosingRangeCancelsBurstAndRestartsReaction) {
    start();
    tick(24);
    EXPECT_EQ(shots, 1);
    world.player.pos = {650, 160};
    tick(1);
    EXPECT_EQ(shots, 1);
    world.player.pos = {160, 160};
    tick(23);
    EXPECT_EQ(shots, 1);
    tick(1);
    EXPECT_EQ(shots, 2);
}
TEST_F(EnemyCombatTest, BlockedSightNeverDamagesAndUnreachablePathHolds) {
    start();
    std::istringstream ascii(
        "############\n#...#......#\n#...#......#\n#...#......#\n#...#......#\n############\n");
    world.level.map = TileMap::parse(ascii, 64);
    world.enemies[0]->pos = {352, 160};
    tick(120);
    EXPECT_EQ(shots, 0);
    EXPECT_FLOAT_EQ(world.player.armor(), 50);
    EXPECT_FLOAT_EQ(world.enemies[0]->pos.x, 352);
}
TEST_F(EnemyCombatTest, ZeroAccuracyFiresWithoutDamage) {
    start(0);
    tick(60);
    EXPECT_EQ(shots, 3);
    EXPECT_FLOAT_EQ(world.player.armor(), 50);
}
TEST_F(EnemyCombatTest, SeededAccuracyMatchesIndependentBulletRolls) {
    start(0.4f);
    Rng expected(42);
    int hits = 0;
    for (int i = 0; i < 3; ++i)
        if (expected.uniformFloat(0, 1) < 0.4f) ++hits;
    tick(39);
    EXPECT_EQ(shots, 3);
    EXPECT_FLOAT_EQ(world.player.armor(), 50 - 7.f * hits);
}
TEST_F(EnemyCombatTest, LosingSightResetsReactionEvenWhenStillInRange) {
    start();
    tick(20);
    std::istringstream ascii(
        "############\n#..#.......#\n#..#.......#\n#..#.......#\n#..#.......#\n############\n");
    world.level.map = TileMap::parse(ascii, 64);
    tick(1);
    world.level.map = arena().map;
    tick(23);
    EXPECT_EQ(shots, 0);
    tick(1);
    EXPECT_EQ(shots, 1);
}
TEST_F(EnemyCombatTest, GuardFightsOnlyAfterAlarmAndDeadGuardCannotActOrDrop) {
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{4, 2}};
    world.guards.emplace_back(spawn, world.level.map, config.guard);
    GuardAI guardAi(world.guards[0], world.level.map, bus, logger);
    for (auto& spec : specs)
        if (spec.id == "patrol_guard") spec.accuracy = 1;
    ai = std::make_unique<EnemyCombatSystem>(bus, world, combat, pickups, specs, config, 42);
    int guardShots = 0;
    bus.subscribe<ShotFired>([&](const auto& event) {
        if (event.shooterId == "G01") ++guardShots;
    });
    tick(60);
    EXPECT_EQ(guardShots, 0);
    bus.publish(AlarmTriggered{AlarmReason::Combat});
    bus.dispatch();
    tick(24);
    EXPECT_EQ(guardShots, 1);
    EXPECT_FLOAT_EQ(world.player.armor(), 44);
    tick(30);
    EXPECT_EQ(guardShots, 2);
    combat.applyDamage(world.guards[0], 60, "Ghost");
    bus.dispatch();
    const Vec2 before = world.guards[0].pos;
    tick(60);
    guardAi.update(1);
    EXPECT_EQ(guardShots, 2);
    EXPECT_TRUE(world.guards[0].dead());
    EXPECT_FLOAT_EQ(world.guards[0].pos.x, before.x);
    EXPECT_TRUE(world.pickups.empty());
    EXPECT_FALSE(world.guards[0].takeDown(bus));
}
TEST_F(EnemyCombatTest, DeadAlertedGuardCannotFinishCallIn) {
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{4, 2}};
    spawn.facing = 180;
    config.guard.fillFar = config.guard.fillNear = 1000;
    world.guards.emplace_back(spawn, world.level.map, config.guard);
    ai = std::make_unique<EnemyCombatSystem>(bus, world, combat, pickups, specs, config, 42);
    DetectionSystem detection(bus, logger, world.guards);
    AlarmDirector alarm(bus, logger, world);
    detection.update(1, world.player, world.level.map, world.guards);
    bus.dispatch();
    ASSERT_EQ(world.guards[0].state(), GuardState::Alerted);
    const float remaining = world.guards[0].callInRemaining();
    combat.applyDamage(world.guards[0], 60, "Ghost");
    bus.dispatch();
    detection.update(10, world.player, world.level.map, world.guards);
    alarm.update();
    EXPECT_FLOAT_EQ(world.guards[0].callInRemaining(), remaining);
    EXPECT_FALSE(world.alarmLoud);
}
TEST_F(EnemyCombatTest, AdvanceClosesDistanceAndStrafeReverses) {
    start(0);
    world.enemies[0]->pos = {600, 160};
    tick(30);
    EXPECT_LT(world.enemies[0]->pos.x, 600);
    world.enemies[0]->pos = {300, 160};
    config.enemyCombat.strafeSpeed = 20;
    tick(30);
    EXPECT_LT(world.enemies[0]->pos.y, 160);
    tick(60);
    const float before = world.enemies[0]->pos.y;
    tick(10);
    EXPECT_GT(world.enemies[0]->pos.y, before);
}
TEST_F(EnemyCombatTest, DeathStopsActionsFadesAndDropsOnlyOnce) {
    start();
    int deaths = 0;
    bus.subscribe<EntityDied>([&](const auto& event) {
        if (event.targetId == "C01") ++deaths;
    });
    combat.applyDamage(*world.enemies[0], 100, "Ghost");
    bus.dispatch();
    const auto count = world.pickups.size();
    EXPECT_EQ(count, 1u);
    const Vec2 before = world.enemies[0]->pos;
    tick(18);
    EXPECT_EQ(shots, 0);
    EXPECT_FLOAT_EQ(world.enemies[0]->pos.x, before.x);
    EXPECT_NEAR(world.enemies[0]->deathOpacity(), 0.5f, 0.001f);
    combat.applyDamage(*world.enemies[0], 100, "Ghost");
    bus.dispatch();
    EXPECT_EQ(world.pickups.size(), count);
    EXPECT_EQ(deaths, 1);
    tick(18);
    EXPECT_NEAR(world.enemies[0]->deathOpacity(), 0, 0.001f);
}
TEST_F(EnemyCombatTest, StrafeCannotMoveCircleThroughWall) {
    start(0);
    config.enemyCombat.strafeSpeed = 80;
    world.player.pos = {160, 78};
    world.enemies[0]->pos = {300, 78};
    tick(10);
    EXPECT_GE(world.enemies[0]->pos.y, 78);
    EXPECT_FLOAT_EQ(world.enemies[0]->pos.x, 300);
}
TEST_F(EnemyCombatTest, PlayerHitsNearestLivingEnemyAndShootsThroughDeadEnemy) {
    start();
    auto spec = world.enemies[0]->spec();
    world.enemies.push_back(std::make_unique<Cop>("C02", Vec2{400, 160}, spec, 14));
    auto gun = weapons[0];
    gun.spreadDeg = 0;
    gun.damage = 100;
    gun.range = 600;
    Rng rng(42);
    const Weapon weapon(gun);
    auto first =
        combat.fire(weapon, ShotRay::aim(world.player.pos, ViewConfig{}.eyeHeight, 0), rng, world);
    EXPECT_EQ(first.pellets[0].targetId, "C01");
    EXPECT_TRUE(world.enemies[0]->dead());
    auto second =
        combat.fire(weapon, ShotRay::aim(world.player.pos, ViewConfig{}.eyeHeight, 0), rng, world);
    EXPECT_EQ(second.pellets[0].targetId, "C02");
    EXPECT_TRUE(world.enemies[1]->dead());
}
TEST_F(EnemyCombatTest, DebugSpawnFailsWhenNoPassableVisiblePointExists) {
    start();
    EXPECT_TRUE(ai->spawnDebugCop());
    std::istringstream ascii("###\n#.#\n###\n");
    world.level.map = TileMap::parse(ascii, 64);
    world.player.pos = {96, 96};
    EXPECT_FALSE(ai->spawnDebugCop());
}
TEST(Config, EnemyCombatInvalidPositiveValuesFallBackAndZeroReactionIsAllowed) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    data["enemy_combat"]["burst_interval"] = 0;
    data["enemy_combat"]["reaction_time"] = 0;
    data["enemy_combat"]["death_fade"] = -1;
    std::ostringstream output;
    Logger logger(output, "");
    const auto config = Config::load(files.write("enemy-tuning.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(config.enemyCombat.burstInterval, 0.12f);
    EXPECT_FLOAT_EQ(config.enemyCombat.deathFade, 0.6f);
    EXPECT_FLOAT_EQ(config.enemyCombat.reactionTime, 0);
    EXPECT_NE(output.str().find("[WARN]"), std::string::npos);
}

TEST_F(EnemyCombatTest, EasyDamageMultiplierAppliesToPoliceBullets) {
    start(1);
    ai = std::make_unique<EnemyCombatSystem>(bus, world, combat, pickups, specs, config, 42,
                                             config.difficulty.easy.enemyDmg);
    const float before = world.player.armor();
    tick(24);
    EXPECT_NEAR(before - world.player.armor(), 7 * config.difficulty.easy.enemyDmg, 1e-4);
}
