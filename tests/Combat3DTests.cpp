#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "systems/CombatSystem.h"
#include "world/World.h"

class Combat3D : public testing::Test {
   protected:
    std::ostringstream output;
    Logger logger{output, ""};
    EventBus events;
    std::vector<WeaponSpec> specs = loadWeapons("assets/config/weapons.json", logger).value();
    std::vector<EnemySpec> enemies = loadEnemies("assets/config/enemies.json", logger).value();
    World arena(const std::string& row = std::string(40, '.')) {
        Level level;
        std::istringstream source(row + '\n' + std::string(row.size(), '.') + '\n' +
                                  std::string(row.size(), '.'));
        level.map = TileMap::parse(source, 48);
        return World(std::move(level), PlayerConfig{});
    }
    Weapon gun(float damage = 10) {
        auto spec = specs[0];
        spec.spreadDeg = 0;
        spec.damage = damage;
        spec.range = 600;
        return Weapon(spec);
    }
    void cop(World& world, std::string id, Vec2 position) {
        world.enemies.push_back(
            std::make_unique<Cop>(std::move(id), position, enemies[1], enemies[1].radius));
    }
};

TEST_F(Combat3D, PitchHitsBodyCapAndMissesAboveBelowOrBehind) {
    auto world = arena();
    cop(world, "cop", {200, 24});
    CombatSystem combat(events, specs, 42);
    Rng rng(42);
    auto hit = combat.fire(gun(), {{24, 36, 24}, {1, 0, 0}}, rng, world);
    ASSERT_EQ(hit.pellets.size(), 1u);
    EXPECT_EQ(hit.pellets[0].impact, ShotImpact::Body);
    EXPECT_EQ(hit.pellets[0].targetId, "cop");
    EXPECT_FLOAT_EQ(hit.pellets[0].to3D.y, 36);
    hit = combat.fire(gun(), {{200, 60, 24}, {0, -1, 0}}, rng, world);
    EXPECT_EQ(hit.pellets[0].targetId, "cop");
    EXPECT_FLOAT_EQ(hit.pellets[0].to3D.y, 48);
    for (const ShotRay ray : {ShotRay{{24, 50, 24}, {1, 0, 0}}, ShotRay{{24, 36, 24}, {-1, 0, 0}},
                              ShotRay{{24, 36, 24}, {1, -1, 0}}}) {
        hit = combat.fire(gun(), ray, rng, world);
        EXPECT_TRUE(hit.pellets[0].targetId.empty());
        EXPECT_FLOAT_EQ(hit.pellets[0].damage, 0);
    }
    EXPECT_FLOAT_EQ(world.enemies[0]->hp(), 80);
}

TEST_F(Combat3D, WallTiesWinAndActorTiesUseStableIdsRegardlessOfStorageOrder) {
    auto world = arena("....#........");
    cop(world, "z", {206, 24});  // cylinder entry at the wall's near face
    CombatSystem combat(events, specs, 42);
    Rng rng(42);
    auto result = combat.fire(gun(), {{24, 36, 24}, {1, 0, 0}}, rng, world);
    EXPECT_EQ(result.pellets[0].impact, ShotImpact::Geometry);
    EXPECT_TRUE(result.pellets[0].targetId.empty());
    world.enemies[0]->pos = {150, 24};
    cop(world, "a", {150, 24});
    result = combat.fire(gun(), {{24, 36, 24}, {1, 0, 0}}, rng, world);
    EXPECT_EQ(result.pellets[0].targetId, "a");
    std::swap(world.enemies[0], world.enemies[1]);
    result = combat.fire(gun(), {{24, 36, 24}, {1, 0, 0}}, rng, world);
    EXPECT_EQ(result.pellets[0].targetId, "a");
}

TEST_F(Combat3D, ShieldInsideBodyProtectsCoveredFrontButNotHeadLegsSidesOrRear) {
    auto world = arena();
    world.enemies.push_back(std::make_unique<ShieldCop>("shield", Vec2{200, 72}, enemies[2]));
    CombatSystem combat(events, specs, 42);
    Rng rng(42);
    auto result = combat.fire(gun(), {{300, 24, 72}, {-1, 0, 0}}, rng, world);
    EXPECT_EQ(result.pellets[0].impact, ShotImpact::Shield);
    EXPECT_NEAR(result.pellets[0].damage, 1, .0001);
    EXPECT_FLOAT_EQ(result.pellets[0].to3D.x, 214);
    for (const ShotRay ray :
         {ShotRay{{300, 47, 72}, {-1, 0, 0}}, ShotRay{{300, 1, 72}, {-1, 0, 0}},
          ShotRay{{100, 24, 72}, {1, 0, 0}}, ShotRay{{200, 24, 140}, {0, 0, -1}}}) {
        result = combat.fire(gun(), ray, rng, world);
        EXPECT_EQ(result.pellets[0].impact, ShotImpact::Body);
        EXPECT_FLOAT_EQ(result.pellets[0].damage, 10);
    }
    EXPECT_NEAR(world.enemies[0]->hp(), 79, .0001);
}

TEST_F(Combat3D, NearerActorAndBlockingWallPreventProtectedShieldDamage) {
    auto world = arena();
    world.enemies.push_back(std::make_unique<ShieldCop>("shield", Vec2{200, 24}, enemies[2]));
    cop(world, "near", {250, 24});
    CombatSystem combat(events, specs, 42);
    Rng rng(42);
    auto result = combat.fire(gun(), {{300, 24, 24}, {-1, 0, 0}}, rng, world);
    EXPECT_EQ(result.pellets[0].targetId, "near");
    EXPECT_FLOAT_EQ(world.enemies[0]->hp(), 120);
    auto blocked = arena(".....#.......");
    blocked.enemies.push_back(std::make_unique<ShieldCop>("shield", Vec2{200, 24}, enemies[2]));
    result = combat.fire(gun(), {{330, 24, 24}, {-1, 0, 0}}, rng, blocked);
    EXPECT_EQ(result.pellets[0].impact, ShotImpact::Geometry);
    EXPECT_TRUE(result.pellets[0].targetId.empty());
    EXPECT_FLOAT_EQ(blocked.enemies[0]->hp(), 120);
}

TEST_F(Combat3D, SpreadIsSeededSolidAngleConeAtHorizontalAndVerticalAim) {
    auto world = arena();
    auto spec = specs[2];
    spec.range = 100;
    CombatSystem combat(events, specs, 42);
    for (const Vec3 direction : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, -1, 0}, Vec3{1, 1, 1}}) {
        const ShotRay ray{{24, 200, 24}, *direction.normalized()};
        Rng first(123), second(123);
        double meanCosine = 0;
        std::size_t count = 0;
        const double limit = std::cos(spec.spreadDeg * 3.14159265358979323846 / 360);
        for (int trial = 0; trial < 500; ++trial) {
            const auto one = combat.fire(Weapon(spec), ray, first, world);
            const auto two = combat.fire(Weapon(spec), ray, second, world);
            ASSERT_EQ(one.pellets.size(), static_cast<std::size_t>(spec.pellets));
            for (std::size_t i = 0; i < one.pellets.size(); ++i) {
                const auto& a = one.pellets[i];
                const auto& b = two.pellets[i];
                const float cosine = a.direction3D.dot(ray.direction);
                EXPECT_GE(cosine, limit - .000001);
                EXPECT_NEAR(a.direction3D.dot(a.direction3D), 1, .000001);
                EXPECT_FLOAT_EQ(a.to3D.x, b.to3D.x);
                EXPECT_FLOAT_EQ(a.to3D.y, b.to3D.y);
                EXPECT_FLOAT_EQ(a.to3D.z, b.to3D.z);
                meanCosine += cosine;
                ++count;
            }
        }
        EXPECT_NEAR(meanCosine / count, (1 + limit) * .5, (1 - limit) * .03);
    }
}

TEST_F(Combat3D, ZeroSpreadStillConsumesTwoSamplesAndInvalidAimDoesNotConsumeAmmo) {
    auto world = arena();
    CombatSystem combat(events, specs, 42);
    Rng first(42), reference(42);
    combat.fire(gun(), {{24, 36, 24}, {1, 0, 0}}, first, world);
    reference.uniformFloat(0, 1);
    reference.uniformFloat(0, 1);
    EXPECT_FLOAT_EQ(first.uniformFloat(0, 1), reference.uniformFloat(0, 1));
    Input input;
    input.firePressed = true;
    input.mouseInViewport = true;
    const int before = combat.activeWeapon().ammunition();
    combat.update(.1f, input, {{24, 36, 24}, {}}, world);
    EXPECT_EQ(combat.activeWeapon().ammunition(), before);
    EXPECT_TRUE(combat.lastShot().pellets.empty());
    EXPECT_TRUE(combat
                    .fire(gun(), {{24, 36, 24}, {0, std::numeric_limits<float>::infinity(), 0}},
                          first, world)
                    .pellets.empty());
}

TEST_F(Combat3D, ShieldPlateFrontalArcIsInclusiveAndOutsideArcTakesFullDamage) {
    auto world = arena();
    world.enemies.push_back(std::make_unique<ShieldCop>("shield", Vec2{200, 72}, enemies[2]));
    CombatSystem combat(events, specs, 42);
    Rng rng(42);
    for (const float degrees : {-61.f, -60.f, 60.f, 61.f}) {
        const float angle = degrees * 3.14159265358979323846f / 180;
        const Vec3 origin{200 + 100 * std::cos(angle), 24, 72 + 100 * std::sin(angle)};
        const ShotRay ray{origin, *(Vec3{214, 24, 72} - origin).normalized()};
        const auto result = combat.fire(gun(), ray, rng, world);
        ASSERT_EQ(result.pellets.size(), 1u);
        EXPECT_NEAR(result.pellets[0].damage, std::abs(degrees) <= 60 ? 1 : 10, .0001);
    }
}

TEST_F(Combat3D, RangeAndOriginInsideWallPreventDamageAndUnconsciousGuardsDoNotBlock) {
    auto world = arena();
    cop(world, "cop", {200, 24});
    CombatSystem combat(events, specs, 42);
    Rng rng(42);
    auto spec = specs[0];
    spec.spreadDeg = 0;
    spec.range = 160;
    auto result = combat.fire(Weapon(spec), {{24, 36, 24}, {1, 0, 0}}, rng, world);
    EXPECT_TRUE(result.pellets[0].targetId.empty());
    EXPECT_EQ(result.pellets[0].impact, ShotImpact::None);
    EXPECT_FLOAT_EQ(world.enemies[0]->hp(), 100);
    GuardSpawn spawn;
    spawn.id = "guard";
    spawn.waypoints = {{2, 0}};
    world.guards.emplace_back(spawn, world.level.map, GuardConfig{});
    ASSERT_TRUE(world.guards[0].takeDown(events));
    result = combat.fire(gun(), {{24, 36, 24}, {1, 0, 0}}, rng, world);
    EXPECT_EQ(result.pellets[0].targetId, "cop");
    auto blocked = arena(".#............");
    cop(blocked, "cop", {200, 24});
    result = combat.fire(gun(), {{72, 36, 24}, {1, 0, 0}}, rng, blocked);
    EXPECT_TRUE(result.pellets[0].targetId.empty());
    EXPECT_FLOAT_EQ(result.pellets[0].to3D.x, 72);
    EXPECT_EQ(result.pellets[0].impact, ShotImpact::Geometry);
}

TEST_F(Combat3D, ShotEventKeepsPlanarProjectionAndBodyRegionsDoNotAddDamageBonuses) {
    auto world = arena();
    cop(world, "cop", {200, 24});
    CombatSystem combat(events, specs, 42);
    Rng rng(42);
    const auto head = combat.fire(gun(), {{24, 47, 24}, {1, 0, 0}}, rng, world);
    const auto leg = combat.fire(gun(), {{24, 1, 24}, {1, 0, 0}}, rng, world);
    EXPECT_FLOAT_EQ(head.pellets[0].damage, leg.pellets[0].damage);
    ShotFired event;
    int count = 0;
    events.subscribe<ShotFired>([&](const auto& shot) {
        event = shot;
        ++count;
    });
    Input input;
    input.firePressed = true;
    input.mouseInViewport = true;
    combat.update(.1f, input, ShotRay::aim({24, 72}, 36, 90, 30), world);
    events.dispatch();
    ASSERT_EQ(count, 1);
    EXPECT_FLOAT_EQ(event.from.x, 24);
    EXPECT_FLOAT_EQ(event.from.y, 72);
    EXPECT_NEAR(event.dir.x, 0, .000001);
    EXPECT_NEAR(event.dir.y, std::sqrt(.75f), .000001);
}

TEST_F(Combat3D, PlateOnlyIntersectionBeyondLargeFiniteRangeCannotDamageTheOfficer) {
    auto world = arena();
    world.enemies.push_back(std::make_unique<ShieldCop>("shield", Vec2{200, 72}, enemies[2]));
    CombatSystem combat(events, specs, 42);
    auto spec = specs[0];
    spec.spreadDeg = 0;
    spec.range = 16777216;
    Rng rng(42);
    const auto result = combat.fire(Weapon(spec), {{214, 16777266, 88}, {0, -1, 0}}, rng, world);
    EXPECT_TRUE(result.pellets[0].targetId.empty());
    EXPECT_EQ(result.pellets[0].impact, ShotImpact::None);
    EXPECT_FLOAT_EQ(world.enemies[0]->hp(), 120);
}
