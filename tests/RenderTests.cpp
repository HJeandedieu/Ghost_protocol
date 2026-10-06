#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "render/Renderer.h"
#include "systems/DetectionSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/RippleSystem.h"
#include "world/LevelLoader.h"
#include "world/World.h"

class Render : public testing::Test {
   protected:
    void SetUp() override {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1280, 720, "Ghost Protocol render test");
        ASSERT_TRUE(IsWindowReady());
    }
    void TearDown() override {
        if (IsWindowReady()) CloseWindow();
    }
};

TEST_F(Render, ShaderCompilesDarknessIsPreservedAndHudRemainsUnprocessed) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    ASSERT_EQ(console.str().find("[ERROR]"), std::string::npos) << console.str();
    std::istringstream source;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    source.str(rows);
    Level level;
    level.name = "Render test";
    level.map = TileMap::parse(source, 48);
    level.map.setLight(2, 6, LightLevel::Lit);
    Player player({640, 360}, PlayerConfig{});
    RippleSystem ripple(PingConfig{}, level.map);
    renderer.beginFrame();
    renderer.drawLevel(level, player, ripple, player.pos, 0, 1, false, 1234);
    renderer.present();
    auto image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto dark = GetImageColor(image, 200, 360);
    const auto lit = GetImageColor(image, 120, 312);
    const auto halo = GetImageColor(image, 680, 360);
    const auto hud = GetImageColor(image, 20, 10);
    EXPECT_LT(dark.g, 30);
    EXPECT_GT(lit.g, 45);
    EXPECT_GT(halo.g, 45);
    EXPECT_EQ(hud.r, 20);
    EXPECT_EQ(hud.g, 22);
    EXPECT_EQ(hud.b, 27);
    UnloadImage(image);
    renderer.setReduceEffects(true);
    renderer.beginFrame();
    renderer.drawLevel(level, player, ripple, player.pos, 0, 1, false, 1234);
    renderer.present();
    image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto plain = GetImageColor(image, 120, 312);
    EXPECT_EQ(plain.r, 30);
    EXPECT_EQ(plain.g, 74);
    EXPECT_EQ(plain.b, 74);
    UnloadImage(image);
}

TEST_F(Render, InteractionPromptAndSixSegmentNoiseMeterRemainReadable) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    Level level;
    std::istringstream source(".....\n.@S..\n.....");
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {1, 1};
    Config config;
    World world(std::move(level), config.player);
    EventBus bus;
    InteractionSystem interaction(bus);
    interaction.loadBank(world, config);
    interaction.update(1, true, world);
    RippleSystem ripple(config.ping, world.level.map);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, world.player.pos, 0, 1, false, 1234);
    renderer.drawInteractionHud(world, interaction, config.noise.walk, config.noise.sprint);
    renderer.present();
    auto image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    for (int segment = 0; segment < 6; ++segment) {
        const auto color = GetImageColor(image, 108 + segment * 24, 140);
        EXPECT_EQ(color.g, segment < 3 ? 143 : 44);
    }
    const auto prompt = GetImageColor(image, 268, 612);
    EXPECT_EQ(prompt.r, 20);
    EXPECT_EQ(prompt.g, 22);
    EXPECT_EQ(prompt.b, 27);
    const auto ring = GetImageColor(image, 960, 620);
    EXPECT_GT(ring.r, 200);
    UnloadImage(image);
}

TEST_F(Render, GuardsRespectDarknessAndAreVisibleInLitRoomsAndOverview) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.map.setLight(2, 6, LightLevel::Lit);
    level.playerSpawn = {13, 7};
    GuardSpawn lit, dark;
    lit.id = "G01";
    lit.mode = PatrolMode::Stationary;
    lit.waypoints = {{2, 6}};
    dark = lit;
    dark.id = "G02";
    dark.waypoints = {{4, 6}};
    level.guards = {lit, dark};
    World world(std::move(level), PlayerConfig{});
    RippleSystem ripple(PingConfig{}, world.level.map);
    renderer.setReduceEffects(true);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234,
                       world.guards);
    renderer.present();
    auto image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_GT(GetImageColor(image, 120, 312).r, 200);
    // A visible neighbor's faint cone may cover the floor, but must not expose this guard.
    EXPECT_LT(GetImageColor(image, 216, 312).r, 100);
    UnloadImage(image);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, true, 1234,
                       world.guards);
    renderer.present();
    image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    // The overview scales this map to 1232 px wide, centering it in the logical frame.
    EXPECT_GT(GetImageColor(image, 163, 329).r, 200);
    UnloadImage(image);
}

TEST_F(Render, ShippedBankPingRendersAndExportsDevelopmentPreview) {
    std::ostringstream console;
    Logger logger(console, "");
    const auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    Renderer renderer(logger);
    ASSERT_EQ(console.str().find("[ERROR]"), std::string::npos) << console.str();
    Player player(level->map.tileCenter(level->playerSpawn), PlayerConfig{});
    RippleSystem ripple(PingConfig{}, level->map);
    std::vector<Entity*> entities;
    ripple.startPing(player.pos, 0.8f);
    ripple.update(0.35f, level->map, entities);
    renderer.beginFrame();
    renderer.drawLevel(*level, player, ripple, player.pos, 0, 1, false, 1234);
    renderer.present();
    auto image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_EQ(image.width, 1280);
    EXPECT_EQ(image.height, 720);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day6-preview.png"));
    UnloadImage(image);
}

TEST_F(Render, RevealedVisionConeStopsAtClosedDoorsAndUsesTargetLighting) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Level level;
    std::string rows;
    for (int y = 0; y < 10; ++y) {
        std::string row(20, '.');
        row[7] = y == 4 ? 'S' : '#';
        rows += row + '\n';
    }
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {1, 1};
    for (int y = 0; y < 10; ++y)
        for (int x = 8; x < 20; ++x) level.map.setLight(x, y, LightLevel::Lit);
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{4, 4}};
    level.guards.push_back(spawn);
    World world(std::move(level), PlayerConfig{});
    RippleSystem ripple(PingConfig{}, world.level.map);
    const auto capture = [&] {
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234,
                           world.guards);
        renderer.present();
        auto image = LoadImageFromTexture(renderer.frameTexture());
        ImageFlipVertical(&image);
        return image;
    };
    auto image = capture();
    EXPECT_LT(GetImageColor(image, 312, 216).r, 15);
    UnloadImage(image);
    std::vector<Entity*> entities{&world.guards.front()};
    ripple.startPing(world.guards.front().pos, 0);
    ripple.update(0.01f, world.level.map, entities);
    ASSERT_GT(world.guards.front().reveal, 0);
    image = capture();
    EXPECT_GT(GetImageColor(image, 312, 216).r, 35);
    EXPECT_LT(GetImageColor(image, 360, 216).r, 15);
    EXPECT_LT(GetImageColor(image, 432, 216).r, 35);
    UnloadImage(image);
    world.level.map.setOpen(7, 4, true);
    image = capture();
    EXPECT_GT(GetImageColor(image, 432, 216).r, 55);
    EXPECT_LT(GetImageColor(image, 540, 216).r, 35);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day9-cone-preview.png"));
    UnloadImage(image);
    Input input;
    input.crouchPressed = true;
    world.player.update(1.0f / 60, input, world.level.map);
    image = capture();
    EXPECT_LT(GetImageColor(image, 360, 216).r, 15);
    EXPECT_GT(GetImageColor(image, 432, 216).r, 55);
    UnloadImage(image);
    // Once the ping reveal expires, the dark guard must stop displaying its cone.
    ripple.update(3.0f, world.level.map, entities);
    ripple.update(3.0f, world.level.map, entities);
    ASSERT_FLOAT_EQ(world.guards.front().reveal, 0);
    image = capture();
    EXPECT_LT(GetImageColor(image, 312, 216).r, 15);
    UnloadImage(image);
}

TEST_F(Render, DetectionPieFillsClockwiseAndStaysHiddenWithItsGuard) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Level level;
    std::string rows;
    for (int y = 0; y < 10; ++y) rows += std::string(20, '.') + '\n';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {1, 1};
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.mode = PatrolMode::Stationary;
    spawn.waypoints = {{4, 4}};
    level.guards.push_back(spawn);
    World world(std::move(level), PlayerConfig{});
    EventBus events;
    DetectionSystem detection(events, logger, world.guards);
    RippleSystem ripple(PingConfig{}, world.level.map);
    world.player.pos = world.guards.front().pos;
    detection.update(0.5f, world.player, world.level.map, world.guards);
    ASSERT_FLOAT_EQ(world.guards.front().detection(), 50);
    world.player.pos = world.player.prevPos;
    const auto capture = [&] {
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234,
                           world.guards);
        renderer.present();
        auto image = LoadImageFromTexture(renderer.frameTexture());
        ImageFlipVertical(&image);
        return image;
    };
    auto image = capture();
    EXPECT_LT(GetImageColor(image, 221, 190).r, 15);
    UnloadImage(image);
    std::vector<Entity*> entities{&world.guards.front()};
    ripple.startPing(world.guards.front().pos, 0);
    ripple.update(0.01f, world.level.map, entities);
    image = capture();
    EXPECT_GT(GetImageColor(image, 221, 190).r, 200);
    EXPECT_LT(GetImageColor(image, 211, 190).r, 35);
    UnloadImage(image);
    world.player.pos = world.guards.front().pos;
    detection.update(0.5f, world.player, world.level.map, world.guards);
    world.player.pos = world.player.prevPos;
    image = capture();
    EXPECT_GT(GetImageColor(image, 211, 190).r, 200);
    EXPECT_GT(GetImageColor(image, 312, 216).r, GetImageColor(image, 312, 216).g);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day10-detection-preview.png"));
    UnloadImage(image);
    ASSERT_TRUE(world.guards.front().takeDown(events));
    image = capture();
    EXPECT_GT(GetImageColor(image, 216, 216).r, 200);
    EXPECT_LT(GetImageColor(image, 312, 216).r, 35);
    EXPECT_LT(GetImageColor(image, 221, 190).r, 35);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day12-body-preview.png"));
    UnloadImage(image);
}

TEST_F(Render, HazardsStayHiddenInLitRoomsUntilRevealedAndCameraConeStopsAtDoor) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Level level;
    std::string rows;
    for (int y = 0; y < 10; ++y) {
        std::string row(20, '.');
        if (y == 4) row[7] = 'S';
        rows += row + '\n';
    }
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.map.fillLight(LightLevel::Lit);
    level.playerSpawn = {1, 1};
    CameraSpawn spawn;
    spawn.id = "C01";
    spawn.position = {4, 4};
    spawn.range = 340;
    level.cameras.push_back(spawn);
    level.lasers.push_back({"L01", {4, 6}, {12, 6}});
    World world(std::move(level), PlayerConfig{});
    RippleSystem ripple(PingConfig{}, world.level.map);
    std::vector<Entity*> hazards{&world.cameras.front(), &world.lasers.front()};
    const auto capture = [&] {
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234,
                           world.guards, world.cameras, world.lasers);
        renderer.present();
        auto image = LoadImageFromTexture(renderer.frameTexture());
        ImageFlipVertical(&image);
        return image;
    };
    auto image = capture();
    EXPECT_LT(GetImageColor(image, 216, 216).r, 35);
    EXPECT_LT(GetImageColor(image, 360, 312).r, 35);
    UnloadImage(image);
    ripple.startPing(world.cameras.front().pos, 0);
    ripple.update(0.2f, world.level.map, hazards);
    image = capture();
    EXPECT_GT(GetImageColor(image, 216, 216).r, 200);
    EXPECT_GT(GetImageColor(image, 312, 216).r, 35);
    EXPECT_LT(GetImageColor(image, 408, 216).r, 35);
    EXPECT_GT(GetImageColor(image, 360, 312).r, 200);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day13-hazards-preview.png"));
    UnloadImage(image);
}

TEST_F(Render, AmbientArchitecturePreservesHiddenMarkersAndGameplayVisibility) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    rows[7 * 41 + 4] = '#';
    rows[7 * 41 + 6] = 'S';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    Player player({640, 360}, PlayerConfig{});
    RippleSystem ripple(PingConfig{}, level.map);
    EXPECT_FLOAT_EQ(ripple.visibility(6, 7, player.pos, level.map), 0);
    renderer.beginFrame();
    renderer.drawLevel(level, player, ripple, player.pos, 0, 1, false, 1234);
    renderer.present();
    auto image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto floor = GetImageColor(image, 312, 360);
    const auto wallMass = GetImageColor(image, 216, 360);
    const auto wallEdge = GetImageColor(image, 193, 360);
    EXPECT_GT(floor.g, 10);
    EXPECT_LT(floor.g, 30);
    EXPECT_EQ(floor.r, GetImageColor(image, 300, 360).r);
    EXPECT_EQ(wallMass.g, 10);
    EXPECT_GT(wallEdge.g, floor.g);
    EXPECT_FLOAT_EQ(ripple.visibility(6, 7, player.pos, level.map), 0);
    UnloadImage(image);
}
