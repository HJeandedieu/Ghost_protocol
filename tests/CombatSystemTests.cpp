#include <gtest/gtest.h>

#include <cmath>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "systems/AlarmDirector.h"
#include "systems/CombatSystem.h"
#include "systems/NoiseSystem.h"
#include "world/World.h"
class CombatSystemTest : public testing::Test {
   protected:
    std::ostringstream console;
    Logger logger{console, ""};
    EventBus bus;
    std::vector<WeaponSpec> specs = loadWeapons("assets/config/weapons.json", logger).value();
    World makeWorld(const std::string& row = "...............") {
        Level level;
        std::istringstream source(row);
        level.map = TileMap::parse(source, 48);
        level.playerSpawn = {0, 0};
        return World(std::move(level), PlayerConfig{});
    }
};
TEST_F(CombatSystemTest, DefaultTwoSlotsSwitchWithKeysAndWheelAndPreserveAmmo) {
    auto world = makeWorld();
    CombatSystem combat(bus, specs, 42);
    Input input;
    input.mouseInViewport = true;
    input.firePressed = true;
    combat.update(1.0f / 60, input, 0, world);
    EXPECT_EQ(combat.activeWeapon().ammunition(), 11);
    input.clearEdges();
    input.weaponSlot = 1;
    combat.update(1.0f / 60, input, 0, world);
    EXPECT_EQ(combat.activeWeapon().spec().id, "chatter");
    EXPECT_EQ(combat.activeWeapon().ammunition(), 30);
    input.clearEdges();
    input.weaponWheel = -1;
    combat.update(1.0f / 60, input, 0, world);
    EXPECT_EQ(combat.activeSlot(), 0);
    EXPECT_EQ(combat.activeWeapon().ammunition(), 11);
    input.clearEdges();
    input.firePressed = true;
    input.mouseInViewport = false;
    combat.update(1, input, 0, world);
    EXPECT_EQ(combat.activeWeapon().ammunition(), 11);
}
TEST_F(CombatSystemTest, HitscanStopsAtWallAndClosedDoorAndFindsClosestCircleBeforeWall) {
    auto world = makeWorld(".....#.........");
    CombatSystem combat(bus, specs, 42);
    auto spec = specs[0];
    spec.spreadDeg = 0;
    Weapon weapon(spec);
    Rng rng(42);
    auto shot = combat.fire(weapon, world.player.pos, 0, rng, world);
    ASSERT_EQ(shot.pellets.size(), 1u);
    EXPECT_FLOAT_EQ(shot.pellets[0].to.x, 240);
    GuardSpawn far;
    far.id = "far";
    far.waypoints = {{4, 0}};
    GuardSpawn near = far;
    near.id = "near";
    near.waypoints = {{2, 0}};
    world.guards.emplace_back(far, world.level.map, GuardConfig{});
    world.guards.emplace_back(near, world.level.map, GuardConfig{});
    shot = combat.fire(weapon, world.player.pos, 0, rng, world);
    EXPECT_EQ(shot.pellets[0].targetId, "near");
    EXPECT_FLOAT_EQ(shot.pellets[0].to.x, world.guards[1].pos.x - world.guards[1].radius);
    auto closed = makeWorld(".....d.........");
    shot = combat.fire(weapon, closed.player.pos, 0, rng, closed);
    EXPECT_FLOAT_EQ(shot.pellets[0].to.x, 240);
    closed.level.map.setOpen(5, 0, true);
    shot = combat.fire(weapon, closed.player.pos, 0, rng, closed);
    EXPECT_FLOAT_EQ(shot.pellets[0].to.x, closed.player.pos.x + spec.range);
}
TEST_F(CombatSystemTest, AllPelletsRespectSeededTotalSpreadAndRange) {
    auto world = makeWorld(std::string(40, '.'));
    CombatSystem combat(bus, specs, 123);
    for (const auto& spec : specs) {
        Weapon weapon(spec);
        Rng a(999), b(999);
        for (int trial = 0; trial < 40; ++trial) {
            const auto one = combat.fire(weapon, world.player.pos, 0, a, world);
            const auto two = combat.fire(weapon, world.player.pos, 0, b, world);
            ASSERT_EQ(one.pellets.size(), static_cast<std::size_t>(spec.pellets));
            for (std::size_t i = 0; i < one.pellets.size(); ++i) {
                EXPECT_GE(one.pellets[i].dirDeg, -spec.spreadDeg * 0.5f);
                EXPECT_LE(one.pellets[i].dirDeg, spec.spreadDeg * 0.5f);
                EXPECT_FLOAT_EQ(one.pellets[i].dirDeg, two.pellets[i].dirDeg);
                EXPECT_LE(std::hypot(one.pellets[i].to.x - world.player.pos.x,
                                     one.pellets[i].to.y - world.player.pos.y),
                          spec.range + 0.001f);
            }
        }
    }
}
TEST_F(CombatSystemTest, AcceptedShotsEmitNoiseAndOnlyHeardUnsuppressedShotsRaiseAlarm) {
    auto world = makeWorld();
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.waypoints = {{5, 0}};
    world.guards.emplace_back(spawn, world.level.map, GuardConfig{});
    AlarmDirector alarm(bus, logger, world);
    NoiseSystem noise(bus);
    noise.setWeapons(specs, NoiseConfig{});
    CombatSystem combat(bus, specs, 42);
    int fired = 0;
    float radius = 0;
    bus.subscribe<ShotFired>([&](const auto& event) {
        ++fired;
        EXPECT_EQ(event.shooterId, world.player.id);
    });
    bus.subscribe<NoiseEmitted>([&](const auto& event) { radius = event.radius; });
    Input input;
    input.firePressed = true;
    input.mouseInViewport = true;
    combat.update(1.0f / 60, input, 0, world);
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(fired, 1);
    EXPECT_FLOAT_EQ(radius, 350);
    EXPECT_EQ(alarm.state(), AlarmState::Quiet);
    combat.update(1.0f / 60, input, 0, world);
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(fired, 1);
    input.weaponSlot = 1;
    combat.update(1.0f / 60, input, 0, world);
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(fired, 2);
    EXPECT_FLOAT_EQ(radius, 900);
    EXPECT_EQ(alarm.state(), AlarmState::Loud);
}
TEST_F(CombatSystemTest, UnsuppressedShotOutsideHearingDoesNotRaiseAlarm) {
    auto world = makeWorld(std::string(40, '.'));
    GuardSpawn spawn;
    spawn.id = "far";
    spawn.waypoints = {{30, 0}};
    world.guards.emplace_back(spawn, world.level.map, GuardConfig{});
    AlarmDirector alarm(bus, logger, world);
    NoiseSystem noise(bus);
    noise.setWeapons(specs, NoiseConfig{});
    CombatSystem combat(bus, specs, 42);
    Input input;
    input.weaponSlot = 1;
    input.firePressed = true;
    input.mouseInViewport = true;
    combat.update(1.0f / 60, input, 0, world);
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(alarm.state(), AlarmState::Quiet);
}
