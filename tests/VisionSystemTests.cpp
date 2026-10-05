#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>

#include "entities/Guard.h"
#include "entities/Player.h"
#include "systems/VisionSystem.h"

namespace {
TileMap openMap() {
    std::string rows;
    for (int y = 0; y < 20; ++y) rows += std::string(20, '.') + '\n';
    std::istringstream source(rows);
    return TileMap::parse(source, 48);
}
}  // namespace

TEST(VisionSystem, InsideOutsideAndInclusiveConeAndRangeBoundaries) {
    const auto map = openMap();
    VisionSystem vision;
    const Vec2 eye{360, 360};
    EXPECT_TRUE(vision.sees(eye, 0, 37.5f, 300, {660, 360}, map));
    EXPECT_FALSE(vision.sees(eye, 0, 37.5f, 300, {661, 360}, map));
    EXPECT_FALSE(vision.sees(eye, 0, 37.5f, 300, {260, 360}, map));
    for (const float angle : {-37.5f, 37.5f}) {
        const float radians = angle * 3.14159265358979323846f / 180;
        EXPECT_TRUE(vision.sees(eye, 0, 37.5f, 300,
                                {eye.x + 200 * std::cos(radians), eye.y + 200 * std::sin(radians)},
                                map));
    }
    EXPECT_FALSE(vision.sees(eye, 0, 37.5f, 300, {500, 500}, map));
    EXPECT_TRUE(vision.sees(eye, 359, 37.5f, 300, {500, 360}, map));
    EXPECT_TRUE(vision.sees(eye, -361, 37.5f, 300, {500, 360}, map));
}

TEST(VisionSystem, WallClosedDoorAndDiagonalCornerBlockSight) {
    VisionSystem vision;
    for (const char obstacle : {'#', 'd', 'S', 'R', 'G', 'V', 'F'}) {
        std::istringstream source(std::string(".....\n..") + obstacle + "..\n.....");
        auto map = TileMap::parse(source, 48);
        EXPECT_FALSE(vision.sees({72, 72}, 0, 37.5f, 300, {168, 72}, map));
        if (obstacle != '#') {
            map.setOpen(2, 1, true);
            EXPECT_TRUE(vision.sees({72, 72}, 0, 37.5f, 300, {168, 72}, map));
        }
    }
    std::istringstream corner(".#.\n#..\n...");
    const auto map = TileMap::parse(corner, 48);
    EXPECT_FALSE(vision.sees({24, 24}, 45, 37.5f, 300, {72, 72}, map));
}

TEST(VisionSystem, LightAndDarkCrouchSelectConfiguredRange) {
    const auto map = openMap();
    VisionSystem vision;
    EXPECT_FLOAT_EQ(vision.rangeFor(LightLevel::Lit, false), 300);
    EXPECT_FLOAT_EQ(vision.rangeFor(LightLevel::Dim, false), 240);
    EXPECT_FLOAT_EQ(vision.rangeFor(LightLevel::Dark, false), 180);
    EXPECT_FLOAT_EQ(vision.rangeFor(LightLevel::Dark, true), 126);
    EXPECT_FLOAT_EQ(vision.rangeFor(LightLevel::Lit, true), 300);
    EXPECT_FLOAT_EQ(vision.rangeFor(LightLevel::Dim, true), 240);
    const Vec2 eye{360, 360};
    EXPECT_TRUE(
        vision.sees(eye, 0, 37.5f, vision.rangeFor(LightLevel::Lit, false), {630, 360}, map));
    EXPECT_FALSE(
        vision.sees(eye, 0, 37.5f, vision.rangeFor(LightLevel::Dim, false), {630, 360}, map));
    EXPECT_TRUE(
        vision.sees(eye, 0, 37.5f, vision.rangeFor(LightLevel::Dim, false), {570, 360}, map));
    EXPECT_FALSE(
        vision.sees(eye, 0, 37.5f, vision.rangeFor(LightLevel::Dark, false), {570, 360}, map));
    EXPECT_FALSE(
        vision.sees(eye, 0, 37.5f, vision.rangeFor(LightLevel::Dark, true), {510, 360}, map));
}

TEST(VisionSystem, InvalidInputsAndOutOfMapCannotSeeTargets) {
    const auto map = openMap();
    VisionSystem vision;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(vision.sees({72, 72}, nan, 37.5f, 300, {90, 72}, map));
    EXPECT_FALSE(vision.sees({72, 72}, 0, -1, 300, {90, 72}, map));
    EXPECT_FALSE(vision.sees({72, 72}, 0, 37.5f, -1, {90, 72}, map));
    EXPECT_FALSE(vision.sees({72, 72}, 0, 37.5f, 300, {nan, 72}, map));
    EXPECT_FALSE(vision.sees({-1, 72}, 0, 37.5f, 300, {90, 72}, map));
    EXPECT_TRUE(vision.sees({72, 72}, 0, 0, 0, {72, 72}, map));
}

TEST(VisionSystem, PlayerCheckUsesTargetLightingAndCrouchInsteadOfGuardLighting) {
    auto map = openMap();
    GuardSpawn spawn;
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{1, 1}};
    Guard guard(spawn, map, GuardConfig{});
    Player player({312, 72}, PlayerConfig{});
    VisionSystem vision;
    map.setLight(6, 1, LightLevel::Lit);
    EXPECT_TRUE(vision.sees(guard, player, map));
    map.setLight(6, 1, LightLevel::Dark);
    EXPECT_FALSE(vision.sees(guard, player, map));
    player.pos = {222, 72};
    EXPECT_TRUE(vision.sees(guard, player, map));
    Input input;
    input.crouchPressed = true;
    player.update(1.0f / 60, input, map);
    EXPECT_FALSE(vision.sees(guard, player, map));
    map.setLight(4, 1, LightLevel::Lit);
    EXPECT_TRUE(vision.sees(guard, player, map));
    player.pos.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(vision.sees(guard, player, map));
}
