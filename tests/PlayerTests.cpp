#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>

#include "core/Logger.h"
#include "entities/Player.h"
#include "world/LevelLoader.h"
#include "world/TileMap.h"

namespace {
TileMap openMap() {
    std::istringstream source(std::string(40, '.') + '\n' + std::string(40, '.') + '\n' +
                              std::string(40, '.'));
    return TileMap::parse(source, 48);
}
void ticks(Player& player, Input input, const TileMap& map, int count) {
    for (int i = 0; i < count; ++i) {
        player.update(1.0f / 60.0f, input, map);
        input.clearEdges();
    }
}
}  // namespace

TEST(Player, AcceleratesToWalkAndDeceleratesToRestFromJsonTuning) {
    const auto map = openMap();
    PlayerConfig config;
    Player player({100, 72}, config);
    Input input;
    input.move = {1, 0};
    player.update(1.0f / 60.0f, input, map);
    EXPECT_NEAR(player.velocity().x, config.accel / 60.0f, 0.001f);
    EXPECT_FLOAT_EQ(player.prevPos.x, 100);
    ticks(player, input, map, 30);
    EXPECT_FLOAT_EQ(player.velocity().x, config.walk);
    input.move = {};
    player.update(1.0f / 60.0f, input, map);
    EXPECT_NEAR(player.velocity().x, config.walk - config.decel / 60.0f, 0.001f);
    ticks(player, input, map, 30);
    EXPECT_FLOAT_EQ(player.velocity().x, 0);
}

TEST(Player, SprintAndCrouchUseConfiguredSpeedsAndCrouchWins) {
    const auto map = openMap();
    PlayerConfig config;
    config.walk = 111;
    config.sprint = 222;
    config.crouch = 55;
    Player player({100, 72}, config);
    Input input;
    input.move = {1, 0};
    input.sprintHeld = true;
    ticks(player, input, map, 30);
    EXPECT_FLOAT_EQ(player.velocity().x, 222);
    input.crouchPressed = true;
    ticks(player, input, map, 30);
    EXPECT_TRUE(player.isCrouched());
    EXPECT_FALSE(player.isSprinting());
    EXPECT_FLOAT_EQ(player.velocity().x, 55);
    input.sprintHeld = false;
    ticks(player, input, map, 30);
    EXPECT_FALSE(player.isCrouched());
    EXPECT_FLOAT_EQ(player.velocity().x, 111);
}

TEST(Player, DiagonalMovementDoesNotExceedWalkSpeed) {
    std::istringstream source(std::string(40, '.') + '\n' + std::string(40, '.') + '\n' +
                              std::string(40, '.') + '\n' + std::string(40, '.') + '\n' +
                              std::string(40, '.'));
    const auto map = TileMap::parse(source, 48);
    PlayerConfig config;
    Player player({100, 72}, config);
    Input input;
    input.move = {1, 1};
    ticks(player, input, map, 30);
    EXPECT_NEAR(std::hypot(player.velocity().x, player.velocity().y), config.walk, 0.001f);
}

TEST(Player, CollisionStopsBlockedAxisAndSlidesAlongWall) {
    std::istringstream source("#####\n#..##\n#..##\n#..##\n#####");
    const auto map = TileMap::parse(source, 48);
    PlayerConfig config;
    Player player({125, 90}, config);
    Input input;
    input.move = {1, 1};
    ticks(player, input, map, 20);
    EXPECT_NEAR(player.pos.x, 144 - config.radius, 0.001f);
    EXPECT_GT(player.pos.y, 110);
    EXPECT_FLOAT_EQ(player.velocity().x, 0);
    EXPECT_GT(player.velocity().y, 0);
}

TEST(Player, InterpolationAndInvalidTickDoNotCorruptState) {
    const auto map = openMap();
    Player player({100, 72}, PlayerConfig{});
    Input input;
    input.move = {1, 0};
    ticks(player, input, map, 1);
    EXPECT_FLOAT_EQ(player.interpolatedPosition(0).x, player.prevPos.x);
    EXPECT_FLOAT_EQ(player.interpolatedPosition(1).x, player.pos.x);
    EXPECT_NEAR(player.interpolatedPosition(0.5f).x, (player.pos.x + player.prevPos.x) * 0.5f,
                0.001f);
    const auto before = player.pos;
    input.crouchPressed = true;
    player.update(0, input, map);
    player.update(-1, input, map);
    player.update(std::numeric_limits<float>::quiet_NaN(), input, map);
    EXPECT_FLOAT_EQ(player.pos.x, before.x);
    EXPECT_FALSE(player.isCrouched());
}

TEST(TileMapCollision, SweptCircleCannotTunnelThroughWallsOrMapEdges) {
    std::istringstream source(".....\n..#..\n.....");
    const auto map = TileMap::parse(source, 48);
    EXPECT_FLOAT_EQ(map.moveCircle({24, 72}, {400, 0}, 14).x, 82);
    EXPECT_FLOAT_EQ(map.moveCircle({216, 72}, {-400, 0}, 14).x, 158);
    EXPECT_FLOAT_EQ(map.moveCircle({24, 24}, {-100, 0}, 14).x, 14);
    EXPECT_FLOAT_EQ(map.moveCircle({24, 24}, {0, -100}, 14).y, 14);
}

TEST(TileMapCollision, CircleClearsRoundedCornersButLockedDoorsBlock) {
    std::istringstream source(".....\n..S..\n.....");
    const auto map = TileMap::parse(source, 48);
    const auto corner = map.moveCircle({72, 40}, {30, 0}, 14);
    EXPECT_NEAR(corner.x, 96 - std::sqrt(196.0f - 64.0f), 0.001f);
    EXPECT_FLOAT_EQ(map.moveCircle({72, 34}, {40, 0}, 14).x, 112);
    EXPECT_FLOAT_EQ(map.moveCircle({72, 72}, {40, 0}, 14).x, 82);
}

TEST(Player, WalksShippedAlleyAndLoadingDockUntilTheLockedServiceDoor) {
    std::ostringstream console;
    Logger logger(console, "");
    const auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    TileCoord service{};
    for (int y = 0; y < level->map.height(); ++y)
        for (int x = 0; x < level->map.width(); ++x)
            if (level->map.tile(x, y) == TileType::ServiceDoor) service = {x, y};
    const auto doorway = level->map.tileCenter(service);
    const auto config = Config::load("assets/config/tuning.json", logger);
    Player player(level->map.tileCenter(level->playerSpawn), config.player);
    Input input;
    input.move = {0, -1};
    for (int i = 0; i < 600 && player.pos.y > doorway.y; ++i)
        player.update(1.0f / 60.0f, input, level->map);
    input.move = {};
    ticks(player, input, level->map, 30);
    EXPECT_NEAR(player.pos.y, doorway.y, config.player.radius);
    input.move = {1, 0};
    ticks(player, input, level->map, 360);
    EXPECT_NEAR(player.pos.x, service.x * level->map.tileSize() - player.radius, 0.001f);
    EXPECT_FLOAT_EQ(player.velocity().x, 0);
}
