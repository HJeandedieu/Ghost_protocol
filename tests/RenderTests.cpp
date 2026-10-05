#include <gtest/gtest.h>

#include <sstream>

#include "core/Logger.h"
#include "entities/Player.h"
#include "render/Renderer.h"
#include "systems/RippleSystem.h"
#include "world/LevelLoader.h"

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
