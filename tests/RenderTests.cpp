#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "render/Renderer.h"
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
    EXPECT_LT(dark.g, 15);
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
    EXPECT_LT(GetImageColor(image, 216, 312).r, 15);
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
