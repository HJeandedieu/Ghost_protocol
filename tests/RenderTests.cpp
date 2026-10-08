#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "render/AlarmSequence.h"
#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "states/BriefingState.h"
#include "states/GameOverState.h"
#include "states/LoadoutState.h"
#include "states/MenuState.h"
#include "states/PauseState.h"
#include "states/PayoutState.h"
#include "states/PlayState.h"
#include "states/StateMachine.h"
#include "systems/CombatSystem.h"
#include "systems/DetectionSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/ObjectiveSystem.h"
#include "systems/RippleSystem.h"
#include "systems/VoiceDirector.h"
#include "systems/WaveSpawner.h"
#include "world/LevelLoader.h"
#include "world/World.h"

namespace {
Image logicalImage(Texture2D texture) {
    auto image = LoadImageFromTexture(texture);
    ImageResize(&image, 1280, 720);
    return image;
}
}  // namespace

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

TEST_F(Render, BriefingSupportsEverySlideBackAndSkipDuringTransitions) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    Input input;
    int starts = 0, backs = 0;
    BriefingState briefing(input, renderer, {}, [&] { ++starts; }, [&] { ++backs; });
    const char* names[] = {"facade", "vault", "van", "tagline"};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(briefing.slide(), i);
        briefing.update(.25f);
        renderer.updateTransition(1);
        renderer.beginFrame();
        briefing.render(0);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        EXPECT_TRUE(ExportImage(
            image, TextFormat(GP_RENDER_OUTPUT_DIRECTORY "/day23-briefing-%s.png", names[i])));
        UnloadImage(image);
        input.confirmPressed = true;
        briefing.update(.016f);
        input.clearEdges();
    }
    EXPECT_EQ(starts, 1);
    input.backPressed = true;
    briefing.update(.016f);
    EXPECT_EQ(briefing.slide(), 2);
    input.clearEdges();
    input.pingPressed = true;
    briefing.update(.016f);
    EXPECT_EQ(starts, 2);
    EXPECT_EQ(backs, 0);
    BriefingState first(input, renderer, {}, [&] { ++starts; }, [&] { ++backs; });
    input.clearEdges();
    input.backPressed = true;
    first.update(.016f);
    EXPECT_EQ(backs, 1);
    renderer.setReduceEffects(true);
    renderer.beginFrame();
    first.render(0);
    renderer.present();
}

TEST_F(Render, EnlargedOutputInterpolatesSurfaceEdgesInsteadOfNearestNeighborSteps) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.beginFrame();
    ClearBackground(BLACK);
    DrawRectangleRec({600.3f, 0, 679.7f, 720}, WHITE);
    renderer.present();
    // Exercise the same scaled texture blit as present(), using a readable GPU
    // target instead of the platform-dependent window swap buffer.
    auto enlarged = LoadRenderTexture(1920, 1080);
    ASSERT_TRUE(IsRenderTextureValid(enlarged));
    BeginTextureMode(enlarged);
    ClearBackground(BLACK);
    DrawTexturePro(renderer.frameTexture(),
                   {0, 0, static_cast<float>(renderer.frameTexture().width),
                    -static_cast<float>(renderer.frameTexture().height)},
                   {0, 0, 1920, 1080}, {0, 0}, 0, WHITE);
    EndTextureMode();
    auto image = LoadImageFromTexture(enlarged.texture);
    ImageFlipVertical(&image);
    EXPECT_EQ(GetImageColor(image, 897, 100).r, 0);
    EXPECT_EQ(GetImageColor(image, 903, 100).r, 255);
    const int edge = 900;
    bool interpolated = false;
    for (int x = edge - 2; x <= edge + 2; ++x) {
        const auto pixel = GetImageColor(image, x, 100);
        interpolated = interpolated || (pixel.r > 4 && pixel.r < 251);
    }
    EXPECT_TRUE(interpolated) << "Enlarged render targets must interpolate edge pixels";
    UnloadImage(image);
    UnloadRenderTexture(enlarged);
}

TEST_F(Render, BriefingUsesSharedHandlerSubtitleWithoutDuplicateCaptionPanel) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Input input;
    BriefingState briefing(input, renderer, {}, [] {}, [] {});
    auto script = loadVoiceLines("assets/config/voice_lines.json", logger);
    ASSERT_TRUE(script);
    VoiceDirector voice(std::move(*script), .25f);
    ASSERT_TRUE(voice.request("V01"));
    for (int slide = 0; slide < 4; ++slide) {
        renderer.beginFrame();
        briefing.render(0);
        renderer.drawVoice(voice, {});
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        const auto outside = GetImageColor(image, 80, 612);
        EXPECT_EQ(outside.r, 10) << "Duplicate caption panel on slide " << slide;
        EXPECT_EQ(outside.g, 10);
        EXPECT_EQ(outside.b, 12);
        EXPECT_GT(GetImageColor(image, 312, 602).r, 180);
        UnloadImage(image);
        input.confirmPressed = true;
        briefing.update(.016f);
        input.clearEdges();
    }
}

TEST_F(Render, PauseSettingsReturnToFrozenPauseAndResumeWithoutReplacingPlay) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    Input input;
    Settings settings;
    StateMachine states;
    int resumes = 0, restarts = 0, menus = 0, saves = 0;
    renderer.beginFrame();
    ClearBackground({30, 74, 74, 255});
    renderer.present();
    renderer.freezeFrame();
    auto pause = std::make_unique<PauseState>(
        input, renderer, UiConfig{},
        std::array<std::function<void()>, 4>{{[&] { ++resumes; },
                                              [&] {
                                                  states.push(std::make_unique<MenuState>(
                                                      input, renderer, UiConfig{}, settings, [] {},
                                                      [&](const Settings& next) {
                                                          settings = next;
                                                          ++saves;
                                                          return true;
                                                      },
                                                      [] {}, "", [&] { states.pop(); }));
                                              },
                                              [&] { ++restarts; }, [&] { ++menus; }}});
    states.push(std::move(pause));
    renderer.beginFrame();
    states.render(0);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day23-pause.png"));
    UnloadImage(image);
    input.menuVertical = 1;
    input.confirmPressed = true;
    states.update(.016f);
    input.clearEdges();
    EXPECT_EQ(states.size(), 2u);
    input.menuHorizontal = -1;
    states.update(.016f);
    input.clearEdges();
    EXPECT_EQ(saves, 1);
    input.backPressed = true;
    states.update(.016f);
    input.clearEdges();
    EXPECT_EQ(states.size(), 1u);
    EXPECT_EQ(resumes, 0);
    input.menuVertical = 1;
    input.confirmPressed = true;
    states.update(.016f);
    input.clearEdges();
    EXPECT_EQ(restarts, 1);
    input.menuVertical = 1;
    input.confirmPressed = true;
    states.update(.016f);
    input.clearEdges();
    EXPECT_EQ(menus, 1);
    input.backPressed = true;
    states.update(.016f);
    EXPECT_EQ(resumes, 1);
}

TEST_F(Render, BustedQuipIsStableAndRetryMenuAndEscapeRemainAvailable) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    Input input;
    int retries = 0, menus = 0;
    renderer.beginFrame();
    ClearBackground({30, 74, 74, 255});
    renderer.present();
    renderer.freezeFrame();
    GameOverState busted(
        input, renderer, {}, "Your lawyer is on hold. Forever.", [&] { ++retries; },
        [&] { ++menus; });
    renderer.beginFrame();
    busted.render(0);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day23-busted.png"));
    UnloadImage(image);
    input.confirmPressed = true;
    busted.update(.016f);
    input.clearEdges();
    EXPECT_EQ(retries, 1);
    input.menuVertical = 1;
    input.confirmPressed = true;
    busted.update(.016f);
    input.clearEdges();
    EXPECT_EQ(menus, 1);
    input.backPressed = true;
    busted.update(.016f);
    EXPECT_EQ(menus, 2);
}

TEST_F(Render, PauseRequestStopsMissionTimeBeforeAnyGameplayTick) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    Config config;
    Input input;
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    auto weapons = loadWeapons("assets/config/weapons.json", logger);
    ASSERT_TRUE(level);
    ASSERT_TRUE(weapons);
    auto run = std::make_shared<MissionRun>();
    run->seconds = 12;
    run->deaths = 2;
    int pausedStage = 0;
    bool pausedLoud = false;
    PlayState play(std::move(*level), input, config, 7, renderer, logger, *weapons, {}, {}, {}, 3,
                   true, {}, run, {}, {{"whisper", "chatter"}}, {}, [&](int stage, bool loud) {
                       pausedStage = stage;
                       pausedLoud = loud;
                   });
    const auto before = play.world().player.pos;
    input.move = {1, 0};
    input.backPressed = true;
    play.update(1.f / 60);
    EXPECT_EQ(pausedStage, 3);
    EXPECT_TRUE(pausedLoud);
    EXPECT_DOUBLE_EQ(run->seconds, 12);
    EXPECT_EQ(run->deaths, 2);
    EXPECT_FLOAT_EQ(play.world().player.pos.x, before.x);
    input.clearEdges();
    input.move = {};
    play.update(1.f / 60);
    EXPECT_GT(run->seconds, 12);
    EXPECT_EQ(run->deaths, 2);
}

TEST_F(Render, PrescribedFontsLoadAndMissingAssetsRetainUsableFallback) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    const auto& assets = renderer.uiAssets();
    ASSERT_TRUE(assets.fontsLoaded());
    ASSERT_NE(assets.logo().id, 0u);
    ASSERT_NE(assets.handlerPortrait().id, 0u);
    renderer.beginFrame();
    assets.text("GHOST PROTOCOL", {64, 64}, 44, {233, 228, 208, 255}, false, true);
    assets.text("START HEIST", {64, 144}, 24, {233, 228, 208, 255});
    assets.text("Are you in or out?", {64, 216}, 22, {233, 228, 208, 255}, true);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day22-fonts.png"));
    UnloadImage(image);
    {
        UiAssets missing(logger, "assets/not-present");
        EXPECT_FALSE(missing.fontsLoaded());
        EXPECT_EQ(missing.body().texture.id, GetFontDefault().texture.id);
        EXPECT_EQ(missing.logo().id, 0u);
        EXPECT_EQ(missing.handlerPortrait().id, 0u);
    }
    EXPECT_NE(GetFontDefault().texture.id, 0u);
    EXPECT_NE(output.str().find("[ERROR] Font unavailable"), std::string::npos);
    EXPECT_NE(output.str().find("Handler portrait unavailable"), std::string::npos);
}

TEST_F(Render, MissionLootUsesRuntimeStateAndBustedRetriesCurrentStage) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    Config config;
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    World world(std::move(*level), config.player);
    ObjectiveSystem::applyPreset(world, config.mission, 5);
    EventBus events;
    InteractionSystem interaction(events);
    interaction.loadBank(world, config);
    AlarmDirector alarm(events, logger, world);
    ObjectiveSystem mission(events, world, interaction, alarm, config, 5);
    world.player.pos = world.player.prevPos = world.level.map.tileCenter({52, 4});
    RippleSystem ripple(config.ping, world.level.map);
    ripple.startPing(world.player.pos, 0);
    auto lootRevealables = mission.revealables();
    ripple.update(0.65f, world.level.map, lootRevealables);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, world.player.pos, 0, 1, false, 7, {}, {},
                       {}, false, nullptr, {}, false, {}, nullptr, nullptr, &mission);
    renderer.drawInteractionHud(world, interaction, 0, config.noise.sprint, &mission);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    // The right vault frame must survive drawing the floor tiles to its right.
    EXPECT_GT(GetImageColor(image, 707, 504).g, 90);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day20-vault-loot.png"));
    UnloadImage(image);

    Input input;
    Config lethal = config;
    lethal.player.hp = 1;
    lethal.player.armor = 0;
    lethal.enemyCombat.reactionTime = 0;
    Level test;
    std::istringstream source("........\n........\n........\n........");
    test.map = TileMap::parse(source, 48);
    test.playerSpawn = {1, 1};
    GuardSpawn guard;
    guard.id = "G01";
    guard.mode = PatrolMode::Stationary;
    guard.waypoints = {{3, 1}};
    test.guards.push_back(guard);
    EnemySpec spec;
    spec.id = "patrol_guard";
    spec.hp = 100;
    spec.engage = 400;
    spec.damage = 10;
    spec.accuracy = 1;
    spec.rate = 10;
    spec.radius = 14;
    int retryStage = 0;
    bool retryLoud = false;
    const auto loadedWeapons = loadWeapons("assets/config/weapons.json", logger);
    ASSERT_TRUE(loadedWeapons);
    auto missionRun = std::make_shared<MissionRun>();
    missionRun->seconds = 12;
    missionRun->deaths = 3;
    PlayState play(
        std::move(test), input, lethal, 7, renderer, logger, *loadedWeapons, {spec}, {}, {}, 2,
        true,
        [&](int stage, bool loud) {
            retryStage = stage;
            retryLoud = loud;
        },
        missionRun);
    EXPECT_EQ(play.world().guards.front().state(), GuardState::Combat);
    EXPECT_TRUE(play.world().alarmLoud);
    for (int tick = 0; tick < 600 && !play.world().player.dead(); ++tick) play.update(1.f / 60);
    ASSERT_TRUE(play.world().player.dead());
    EXPECT_EQ(missionRun->deaths, 4);
    EXPECT_TRUE(missionRun->alarmEver);
    const auto deathTime = missionRun->seconds;
    EXPECT_GT(deathTime, 12);
    for (int i = 0; i < 60; ++i) play.update(1.f / 60);
    EXPECT_DOUBLE_EQ(missionRun->seconds, deathTime);
    EXPECT_EQ(missionRun->deaths, 4);
    renderer.beginFrame();
    play.render(1);
    renderer.present();
    image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day20-busted.png"));
    UnloadImage(image);
    input.confirmPressed = true;
    play.update(1.f / 60);
    EXPECT_EQ(retryStage, 2);
    EXPECT_TRUE(retryLoud);
}

TEST_F(Render, ThermiteDeviceTimerAndSparksRenderAtTheInteractionDoor) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    Config config;
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    World world(std::move(*level), config.player);
    ObjectiveSystem::applyPreset(world, config.mission, 4);
    world.player.pos = world.player.prevPos = world.level.map.tileCenter({52, 8});
    EventBus events;
    InteractionSystem interaction(events);
    interaction.loadBank(world, config);
    AlarmDirector alarm(events, logger, world);
    ObjectiveSystem mission(events, world, interaction, alarm, config, 4);
    interaction.update(config.mission.thermitePlace, true, world, true);
    mission.update(config.mission.thermitePlace);
    events.dispatch();
    ASSERT_FLOAT_EQ(mission.thermiteRemaining(), 75);
    EXPECT_FLOAT_EQ(mission.vaultPosition().x, world.level.map.tileCenter({52, 7}).x);
    RippleSystem ripple(config.ping, world.level.map);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, world.player.pos, 0, 1, false, 7, {}, {},
                       {}, false, nullptr, {}, true, {}, nullptr, nullptr, &mission);
    renderer.drawInteractionHud(world, interaction, 0, config.noise.sprint, &mission);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day20-thermite.png"));
    EXPECT_GT(GetImageColor(image, 640, 312).r, 100);
    EXPECT_GT(GetImageColor(image, 707, 300).g, 150);
    bool timerAboveDoor = false;
    for (int y = 272; y < 288; ++y)
        for (int x = 576; x < 704; ++x) {
            const auto pixel = GetImageColor(image, x, y);
            timerAboveDoor = timerAboveDoor || (pixel.r > 180 && pixel.g > 100 && pixel.b < 80);
        }
    EXPECT_TRUE(timerAboveDoor);
    UnloadImage(image);
}

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
    World world(level, PlayerConfig{});
    EventBus events;
    InteractionSystem interaction(events);
    renderer.beginFrame();
    renderer.drawLevel(level, player, ripple, player.pos, 0, 1, false, 1234);
    renderer.drawInteractionHud(world, interaction, 0, 280);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto dark = GetImageColor(image, 200, 360);
    const auto lit = GetImageColor(image, 120, 312);
    const auto halo = GetImageColor(image, 680, 360);
    const auto hud = GetImageColor(image, 30, 100);
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
    image = logicalImage(renderer.frameTexture());
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
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    bool segmentsFound = false;
    for (int y = 160; y < 240; ++y) {
        bool matches = true;
        for (int segment = 0; segment < 6; ++segment)
            matches = matches &&
                      GetImageColor(image, 108 + segment * 20, y).g == (segment < 3 ? 143 : 44);
        segmentsFound = segmentsFound || matches;
    }
    EXPECT_TRUE(segmentsFound);
    const auto prompt = GetImageColor(image, 316, 516);
    EXPECT_EQ(prompt.r, 20);
    EXPECT_EQ(prompt.g, 22);
    EXPECT_EQ(prompt.b, 27);
    const auto ring = GetImageColor(image, 960, 524);
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
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_GT(GetImageColor(image, 120, 312).r, 200);
    // A visible neighbor's faint cone may cover the floor, but must not expose this guard.
    EXPECT_LT(GetImageColor(image, 216, 312).r, 100);
    UnloadImage(image);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, true, 1234,
                       world.guards);
    renderer.present();
    image = logicalImage(renderer.frameTexture());
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
    auto image = logicalImage(renderer.frameTexture());
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
        auto image = logicalImage(renderer.frameTexture());
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
        auto image = logicalImage(renderer.frameTexture());
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
        auto image = logicalImage(renderer.frameTexture());
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
    auto image = logicalImage(renderer.frameTexture());
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

TEST_F(Render, WeaponTracerStopsAtWallAndAmmoPanelReflectsShotAndReload) {
    std::ostringstream console;
    Logger logger(console, "");
    auto specs = loadWeapons("assets/config/weapons.json", logger);
    ASSERT_TRUE(specs);
    specs->front().spreadDeg = 0;
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    rows[7 * 41 + 17] = '#';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {13, 7};
    World world(std::move(level), PlayerConfig{});
    EventBus bus;
    CombatSystem combat(bus, *specs, 42);
    Input input;
    input.firePressed = true;
    input.mouseInViewport = true;
    combat.update(1.0f / 60, input, 0, world);
    EXPECT_EQ(combat.activeWeapon().ammunition(), 11);
    Renderer renderer(logger);
    RippleSystem ripple(PingConfig{}, world.level.map);
    const auto draw = [&] {
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234, {}, {},
                           {}, false, &combat);
        renderer.drawWeaponHud(combat);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        return image;
    };
    auto image = draw();
    const auto tracer = GetImageColor(image, 780, 360);
    EXPECT_GT(tracer.r, 200);
    EXPECT_GT(tracer.g, 150);
    EXPECT_GT(GetImageColor(image, 675, 357).r, 180);
    EXPECT_GT(GetImageColor(image, 648, 360).g, 200);
    EXPECT_LT(GetImageColor(image, 850, 360).r, 40);
    EXPECT_EQ(GetImageColor(image, 30, 580).r, 20);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day15-weapons-preview.png"));
    UnloadImage(image);
    input.clearEdges();
    input.reloadPressed = true;
    combat.update(0.1f, input, 0, world);
    EXPECT_GT(combat.activeWeapon().reloadRemaining(), 0);
    image = draw();
    EXPECT_LT(GetImageColor(image, 780, 360).r, 40);
    UnloadImage(image);
}

TEST_F(Render, HealthAndArmorBarsDrainWithWorldHitFlashAndRemainStableWithGuards) {
    std::ostringstream console;
    Logger logger(console, "");
    auto specs = loadWeapons("assets/config/weapons.json", logger);
    ASSERT_TRUE(specs);
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {13, 7};
    World world(std::move(level), PlayerConfig{});
    GuardSpawn spawn;
    spawn.id = "G01";
    spawn.waypoints = {{30, 7}};
    world.guards.emplace_back(spawn, world.level.map, GuardConfig{});
    EventBus bus;
    CombatSystem combat(bus, *specs, 42);
    Renderer renderer(logger);
    renderer.resetHealthHud(world.player);
    bus.subscribe<EntityDamaged>([&](const auto&) { renderer.notifyHealthDamage(); });
    combat.applyDamage(world.player, 65, "cop");
    bus.dispatch();
    renderer.updateHealthHud(0.15f, world.player);
    RippleSystem ripple(PingConfig{}, world.level.map);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234,
                       world.guards);
    renderer.drawWeaponHud(combat);
    RecoveryPickup pickup("fixture", PickupType::Medkit, world.player.pos);
    renderer.drawPickupHud(&pickup);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto health = GetImageColor(image, 80, 652);
    EXPECT_EQ(health.r, 255);
    EXPECT_EQ(health.g, 59);
    const auto armor = GetImageColor(image, 100, 682);
    EXPECT_EQ(armor.r, 233);
    EXPECT_EQ(armor.g, 228);
    EXPECT_EQ(GetImageColor(image, 180, 682).r, 40);
    EXPECT_EQ(GetImageColor(image, 274, 682).r, 40);
    EXPECT_GT(GetImageColor(image, 400, 200).r, 20);
    EXPECT_EQ(GetImageColor(image, 30, 580).r, 20);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day16-health-preview.png"));
    EXPECT_FLOAT_EQ(world.player.hp(), 85);
    EXPECT_FLOAT_EQ(world.player.armor(), 0);
    UnloadImage(image);
}

TEST_F(Render, RecoveryMarkersRemainHiddenInStealthAndShowDistinctShapesInLoud) {
    std::ostringstream console;
    Logger logger(console, "");
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {13, 7};
    World world(std::move(level), PlayerConfig{});
    RippleSystem ripple(PingConfig{}, world.level.map);
    Renderer renderer(logger);
    const auto draw = [&](bool loud) {
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234, {}, {},
                           {}, false, nullptr, world.pickups, loud);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        return image;
    };
    auto baseline = draw(false);
    world.pickups.push_back(
        std::make_unique<RecoveryPickup>("medkit", PickupType::Medkit, Vec2{900, 360}));
    world.pickups.push_back(
        std::make_unique<RecoveryPickup>("plate", PickupType::ArmorPlate, Vec2{960, 360}));
    auto hidden = draw(false);
    EXPECT_EQ(GetImageColor(hidden, 900, 360).r, GetImageColor(baseline, 900, 360).r);
    EXPECT_EQ(GetImageColor(hidden, 960, 360).r, GetImageColor(baseline, 960, 360).r);
    UnloadImage(baseline);
    UnloadImage(hidden);
    auto loud = draw(true);
    EXPECT_GT(GetImageColor(loud, 900, 360).r, 200);
    EXPECT_GT(GetImageColor(loud, 960, 360).g, 150);
    EXPECT_FLOAT_EQ(world.pickups.front()->reveal, 0);
    EXPECT_TRUE(ExportImage(loud, GP_RENDER_OUTPUT_DIRECTORY "/day16-pickups-preview.png"));
    UnloadImage(loud);
}
TEST_F(Render, PoliceSilhouettesAndWaveHudReadStateWithoutRevealingEnemies) {
    std::ostringstream console;
    Logger logger(console, "");
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {13, 7};
    World world(std::move(level), PlayerConfig{});
    const auto specs = loadEnemies("assets/config/enemies.json", logger).value();
    const auto waveSpecs = loadWaves("assets/config/enemies.json", logger).value();
    EventBus events;
    WaveSpawner waves(events, world, specs, waveSpecs,
                      {{"front", {5, 7}}, {"service", {6, 7}}, {"east", {7, 7}}}, AlarmConfig{});
    world.alarmLoud = true;
    waves.update(30, {-1000, -1000, -900, -900});
    world.enemies.push_back(std::make_unique<ShieldCop>("shield", Vec2{900, 360}, specs[2]));
    world.enemies.push_back(std::make_unique<Heavy>("heavy", Vec2{980, 360}, specs[3]));
    Renderer renderer(logger);
    renderer.resetHealthHud(world.player);
    RippleSystem ripple(PingConfig{}, world.level.map);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, {640, 360}, 0, 1, false, 1234, {}, {}, {},
                       false, nullptr, {}, true, world.enemies);
    renderer.drawWaveHud(waves);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_GT(GetImageColor(image, 920, 360).r, 150);  // Bone shield arc faces right.
    EXPECT_GT(GetImageColor(image, 980, 360).r, 150);  // Gold armored vest stripe.
    EXPECT_EQ(GetImageColor(image, 1015, 165).r, 20);  // Stable framed HUD background.
    EXPECT_EQ(waves.waveIndex(), 1);
    for (const auto& enemy : world.enemies) EXPECT_FLOAT_EQ(enemy->reveal, 0);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day18-police-preview.png"));
    UnloadImage(image);
}

TEST_F(Render, PlayStateStartsAssaultClockAfterAlarmAndDisplaysWaveHud) {
    std::ostringstream console;
    Logger logger(console, "");
    Renderer renderer(logger);
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 64);
    level.playerSpawn = {10, 7};
    level.guards.push_back({"G01", "fixture", PatrolMode::Stationary, false, {{8, 7}}, 0});
    Config config;
    auto enemies = loadEnemies("assets/config/enemies.json", logger).value();
    for (auto& enemy : enemies) enemy.accuracy = 0;
    Input input;
    PlayState play(std::move(level), input, config, 42, renderer, logger,
                   loadWeapons("assets/config/weapons.json", logger).value(), enemies,
                   loadWaves("assets/config/enemies.json", logger).value(),
                   {{"front", {30, 7}}, {"service", {31, 7}}, {"east", {32, 7}}});
    renderer.beginFrame();
    play.render(1);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto quiet = GetImageColor(image, 1100, 244);
    UnloadImage(image);
    input.weaponSlot = 1;
    input.firePressed = true;
    input.mouseInViewport = true;
    input.mouseLogical = {640, 0};
    play.update(1.f / 60);
    input.clearEdges();
    play.update(1.f / 60);  // ShotFired queues NoiseEmitted for the following dispatch.
    ASSERT_NE(console.str().find("Alarm triggered"), std::string::npos);
    for (int i = 0; i < 1800; ++i) play.update(1.f / 60);
    renderer.beginFrame();
    play.render(1);
    renderer.present();
    image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto assault = GetImageColor(image, 1100, 244);
    EXPECT_EQ(assault.r, 20);
    EXPECT_EQ(assault.g, 22);
    EXPECT_EQ(assault.b, 27);
    EXPECT_TRUE(quiet.r != assault.r || quiet.g != assault.g || quiet.b != assault.b);
    ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day18-runtime-preview.png");
    renderer.beginFrame();
    DrawRectangle(1010, 160, 246, 88, {20, 22, 27, 255});
    renderer.uiAssets().text("ASSAULT  1", {1026, 176}, 20, {242, 183, 5, 255}, false, true);
    renderer.present();
    auto expected = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&expected);
    for (int y = 176; y < 200; ++y)
        for (int x = 1026; x < 1220; ++x) {
            const auto actualPixel = GetImageColor(image, x, y);
            const auto expectedPixel = GetImageColor(expected, x, y);
            ASSERT_NEAR(actualPixel.r, expectedPixel.r, 24) << "Assault label at " << x << "," << y;
            ASSERT_NEAR(actualPixel.g, expectedPixel.g, 24) << "Assault label at " << x << "," << y;
            ASSERT_NEAR(actualPixel.b, expectedPixel.b, 24) << "Assault label at " << x << "," << y;
        }
    UnloadImage(expected);
    UnloadImage(image);
}

TEST_F(Render, LoudIlluminatesWallFacesKeepsMassDarkAndHidesNoiseAndPingEffects) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) {
        std::string row(40, '.');
        if (y == 7) row[4] = '#';
        rows += row + '\n';
    }
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {13, 7};
    World world(std::move(level), PlayerConfig{});
    world.alarmLoud = true;
    world.player.pos = world.player.prevPos = {640, 360};
    RippleSystem ripple(PingConfig{}, world.level.map);
    ripple.startPing(world.player.pos, 0);
    EventBus bus;
    InteractionSystem interaction(bus);
    renderer.beginFrame();
    renderer.drawLevel(world.level, world.player, ripple, world.player.pos, 0, 1, false, 42, {}, {},
                       {}, false, nullptr, {}, true);
    renderer.drawInteractionHud(world, interaction, 1000, 500);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto wall = GetImageColor(image, 200, 360);
    EXPECT_GT(wall.r, 180);
    EXPECT_GT(wall.r, wall.g * 3);
    const auto mass = GetImageColor(image, 216, 360);
    EXPECT_LT(mass.r, wall.r * .6f);
    EXPECT_LT(mass.g, 40);
    for (const int x : {300, 680}) {
        const auto floor = GetImageColor(image, x, 360);
        EXPECT_EQ(floor.r, 122);
        EXPECT_EQ(floor.g, 26);
        EXPECT_EQ(floor.b, 43);
    }
    const auto rim = GetImageColor(image, 655, 360);
    EXPECT_NEAR(rim.r, 233, 5);
    EXPECT_NEAR(rim.g, 228, 5);
    const auto noise = GetImageColor(image, 108, 168);
    EXPECT_EQ(noise.r, 20);
    EXPECT_EQ(noise.g, 22);
    EXPECT_EQ(noise.b, 27);
    EXPECT_FLOAT_EQ(ripple.tileReveal(4, 7), 0);
    ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day19-loud-preview.png");
    UnloadImage(image);
}

TEST_F(Render, AlarmPaletteBarsAndBannerRespectRealTimeAndStableHud) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Level level;
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {13, 7};
    World world(std::move(level), PlayerConfig{});
    world.player.pos = world.player.prevPos = {640, 360};
    world.alarmLoud = true;
    RippleSystem ripple(PingConfig{}, world.level.map);
    EventBus bus;
    InteractionSystem interaction(bus);
    AlarmSequence sequence(bus, AlarmConfig{}, 42);
    bus.publish(AlarmTriggered{AlarmReason::Shot});
    bus.dispatch();
    auto draw = [&]() {
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, world.player.pos, 0, 1, false, 42, {},
                           {}, {}, false, nullptr, {}, true, {}, nullptr, &sequence);
        renderer.drawInteractionHud(world, interaction, 1000, 500);
        renderer.drawAlarmSequence(sequence);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        return image;
    };
    sequence.advance(0.2f);
    auto image = draw();
    const auto floor = GetImageColor(image, 300, 360);
    EXPECT_EQ(floor.r, 76);
    EXPECT_EQ(floor.g, 50);
    EXPECT_EQ(floor.b, 58);
    // Avoid the variable-width FPS text when probing the stable HUD background.
    EXPECT_EQ(GetImageColor(image, 200, 20).r, 255);
    EXPECT_EQ(GetImageColor(image, 100, 4).r, 10);
    EXPECT_EQ(GetImageColor(image, 30, 100).r, 20);
    ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day19-alarm-midpoint.png");
    UnloadImage(image);
    sequence.advance(2.4f);
    image = draw();
    EXPECT_EQ(GetImageColor(image, 300, 360).r, 122);
    EXPECT_EQ(GetImageColor(image, 200, 20).r, 122);
    EXPECT_EQ(GetImageColor(image, 100, 4).r, 122);
    ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day19-alarm-settled.png");
    UnloadImage(image);
}

TEST_F(Render, AlarmSequenceRendersShippedBankWithoutWritingReveal) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    World world(std::move(*level), PlayerConfig{});
    world.player.pos = world.player.prevPos = world.level.map.tileCenter({50, 32});
    RippleSystem ripple(PingConfig{}, world.level.map);
    EventBus bus;
    AlarmDirector alarm(bus, logger, world);
    AlarmSequence sequence(bus, AlarmConfig{}, 42);
    alarm.trigger(AlarmReason::Pager);
    bus.dispatch();
    for (const auto& stage : {std::pair<float, const char*>{0.2f, "day19-bank-midpoint.png"},
                              {0.4f, "day19-bank-bars.png"},
                              {2.0f, "day19-bank-loud.png"}}) {
        sequence.advance(stage.first);
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, world.player.pos, 0, 1, false, 42,
                           world.guards, world.cameras, world.lasers, false, nullptr, {}, true, {},
                           nullptr, &sequence);
        renderer.drawAlarmSequence(sequence);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        const std::string path = std::string(GP_RENDER_OUTPUT_DIRECTORY) + "/" + stage.second;
        EXPECT_TRUE(ExportImage(image, path.c_str()));
        UnloadImage(image);
    }
    for (const auto& guard : world.guards) EXPECT_FLOAT_EQ(guard.reveal, 0);
    for (const auto& camera : world.cameras) EXPECT_FLOAT_EQ(camera.reveal, 0);
    for (const auto& laser : world.lasers) EXPECT_FLOAT_EQ(laser.reveal, 0);
}

TEST_F(Render, LoadoutChoicesAndPayoutReturnRemainResponsive) {
    Input input;
    std::array<std::string, 2> chosen;
    bool easy = false;
    int starts = 0;
    LoadoutState loadout(input, [&](auto weapons, bool tourist) {
        chosen = weapons;
        easy = tourist;
        ++starts;
    });
    input.loadoutExcluded = 0;
    input.crouchPressed = true;
    loadout.update(.01f);
    input.clearEdges();
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.beginFrame();
    loadout.render(0);
    renderer.present();
    auto screenshot = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&screenshot);
    ExportImage(screenshot, GP_RENDER_OUTPUT_DIRECTORY "/day21-loadout.png");
    UnloadImage(screenshot);
    input.confirmPressed = true;
    loadout.update(.01f);
    EXPECT_EQ(starts, 1);
    EXPECT_TRUE(easy);
    EXPECT_EQ(chosen[0], "chatter");
    EXPECT_EQ(chosen[1], "gavel");
    PayoutConfig config;
    ScoreSystem score(config);
    score.addBag(200000);
    Rng rng(8);
    auto payout = score.finalize(true, 599, 1, rng);
    int menus = 0;
    PayoutState result(input, renderer, UiConfig{}, payout, [&] { ++menus; }, [&] { ++menus; });
    input.confirmPressed = false;
    result.update(5);
    renderer.beginFrame();
    result.render(0);
    renderer.present();
    screenshot = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&screenshot);
    ExportImage(screenshot, GP_RENDER_OUTPUT_DIRECTORY "/day21-payout.png");
    UnloadImage(screenshot);
    input.confirmPressed = true;
    result.update(.01f);
    EXPECT_EQ(menus, 1);
}

TEST_F(Render, EscapeVanAppearsAfterArrivalAndLoweredBollardsBecomeRecessedDots) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Config config;
    Level level;
    std::istringstream ascii(".............\n..N.b.Z.v....\n.............");
    level.map = TileMap::parse(ascii, 48);
    level.map.fillLight(LightLevel::Lit);
    level.playerSpawn = {2, 1};
    World world(std::move(level), config.player);
    EventBus events;
    InteractionSystem interaction(events);
    interaction.loadBank(world, config);
    AlarmDirector alarm(events, logger, world);
    ObjectiveSystem mission(events, world, interaction, alarm, config, 6);
    RippleSystem ripple(config.ping, world.level.map);
    const auto van = world.level.map.tileCenter({8, 1});
    renderer.prepareLevel(world.level);
    const auto capture = [&] {
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, van, 0, 1, false, 7, {}, {}, {},
                           false, nullptr, {}, false, {}, nullptr, nullptr, &mission);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        return image;
    };
    auto before = capture();
    EXPECT_LT(GetImageColor(before, 664, 360).g, 120);
    UnloadImage(before);
    interaction.update(config.mission.bollardHold, true, world);
    mission.update(config.mission.bollardHold);
    EXPECT_TRUE(world.level.map.isOpen(4, 1));
    EXPECT_FALSE(mission.vanArrived());
    mission.update(config.mission.vanDelay);
    auto after = capture();
    EXPECT_GT(GetImageColor(after, 664, 360).g, 120);
    EXPECT_TRUE(ExportImage(after, GP_RENDER_OUTPUT_DIRECTORY "/day21-escape-van.png"));
    UnloadImage(after);
}

TEST_F(Render, MenuControlsSaveSettingsAndRemainResponsiveDuringTransitions) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    Input input;
    Settings saved;
    int saves = 0, starts = 0;
    bool accept = true;
    MenuState menu(
        input, renderer, UiConfig{}, saved, [&] { ++starts; },
        [&](const Settings& settings) {
            ++saves;
            if (accept) saved = settings;
            return accept;
        },
        [] {});
    auto capture = [&](const char* name) {
        renderer.updateTransition(1);
        renderer.beginFrame();
        menu.render(1);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        EXPECT_TRUE(ExportImage(image, name));
        UnloadImage(image);
    };
    menu.update(.2f);
    capture(GP_RENDER_OUTPUT_DIRECTORY "/day22-menu.png");
    input.menuVertical = 1;
    menu.update(.016f);
    input.clearEdges();
    input.confirmPressed = true;
    menu.update(.016f);
    input.clearEdges();  // Open settings, incoming transition still active.
    input.menuHorizontal = -1;
    menu.update(.016f);
    input.clearEdges();
    EXPECT_FLOAT_EQ(saved.volumeMaster, .79f);
    EXPECT_EQ(saves, 1);
    input.mouseInViewport = true;
    input.mouseLogical = {776, 232};
    input.startClicked = true;
    menu.update(.016f);
    input.clearEdges();
    EXPECT_FLOAT_EQ(saved.volumeMusic, .5f);
    input.mouseLogical = {300, 576};
    input.startClicked = true;
    menu.update(.016f);
    input.clearEdges();
    EXPECT_FALSE(saved.reduceEffects);  // Difficulty changes separately from Reduce Effects.
    EXPECT_EQ(saved.difficulty, "easy");
    capture(GP_RENDER_OUTPUT_DIRECTORY "/day22-settings.png");
    const float previous = saved.volumeMaster;
    accept = false;
    input.mouseLogical = {640, 176};
    input.startClicked = true;
    menu.update(.016f);
    input.clearEdges();
    EXPECT_FLOAT_EQ(saved.volumeMaster, previous);
    input.backPressed = true;
    menu.update(.016f);
    input.clearEdges();
    input.confirmPressed = true;
    menu.update(.016f);
    input.clearEdges();
    EXPECT_EQ(starts, 1);
}

TEST_F(Render, HandlerSubtitleWrapsInsidePanelAndReducedEffectsFreezesPanicAppearance) {
    std::ostringstream out;
    Logger logger(out, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto script = loadVoiceLines("assets/config/voice_lines.json", logger);
    ASSERT_TRUE(script);
    for (auto& line : *script)
        if (line.id == "V08")
            line.text +=
                " Keep moving through the hall, find the vault, and use the doorway for cover.";
    VoiceDirector voice(std::move(*script), .25f);
    voice.request("V08");
    renderer.beginFrame();
    renderer.drawVoice(voice, {});
    renderer.present();
    auto first = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&first);
    const auto border = GetImageColor(first, 312, 602);
    EXPECT_GT(border.r, 180);
    bool secondRow = false;
    for (int y = 644; y < 670; ++y)
        for (int x = 432; x < 1096; ++x)
            secondRow = secondRow || GetImageColor(first, x, y).r > 200;
    EXPECT_TRUE(secondRow);
    for (int y = 600; y < 696; ++y) EXPECT_LT(GetImageColor(first, 1160, y).r, 200);

    EXPECT_TRUE(ExportImage(first, GP_RENDER_OUTPUT_DIRECTORY "/day26-handler-subtitles.png"));
    voice.update(.15f);
    renderer.beginFrame();
    renderer.drawVoice(voice, {});
    renderer.present();
    auto second = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&second);
    const auto next = GetImageColor(second, 312, 602);
    EXPECT_EQ(border.r, next.r);
    EXPECT_EQ(border.g, next.g);
    EXPECT_EQ(border.b, next.b);
    UnloadImage(first);
    UnloadImage(second);
}

TEST_F(Render, Day26BankAndHandlerRemainReadableInStealthAndLoud) {
    std::ostringstream out;
    Logger logger(out, "");
    Config config = Config::load("assets/config/tuning.json", logger);
    Renderer renderer(logger, config.render);
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    World world(std::move(*level), config.player, config.guard, config.camera);
    renderer.prepareLevel(world.level);
    renderer.resetHealthHud(world.player);
    world.player.pos = world.player.prevPos = world.level.map.tileCenter({50, 32});
    RippleSystem ripple(config.ping, world.level.map);
    ripple.startPing(world.player.pos, config.ping.chargeMax);
    std::vector<Entity*> entities;
    for (auto& guard : world.guards) entities.push_back(&guard);
    ripple.update(.55f, world.level.map, entities);
    EventBus events;
    InteractionSystem interaction(events);
    interaction.loadBank(world, config);
    auto script = loadVoiceLines("assets/config/voice_lines.json", logger);
    ASSERT_TRUE(script);
    VoiceDirector voice(std::move(*script), config.voice.lowHealthFraction);
    voice.request("V05");
    for (bool loud : {false, true}) {
        world.alarmLoud = loud;
        renderer.beginFrame();
        renderer.drawLevel(world.level, world.player, ripple, world.player.pos, 0, 1, false, 42,
                           world.guards, world.cameras, world.lasers, false, nullptr, {}, loud);
        renderer.drawInteractionHud(world, interaction, config.noise.walk, config.noise.sprint);
        renderer.drawVoice(voice, config.ui);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        EXPECT_GT(GetImageColor(image, 312, 602).r, 180);
        EXPECT_TRUE(ExportImage(image, loud ? GP_RENDER_OUTPUT_DIRECTORY "/day26-loud.png"
                                            : GP_RENDER_OUTPUT_DIRECTORY "/day26-stealth.png"));
        UnloadImage(image);
    }
    for (const auto& guard : world.guards) EXPECT_GE(guard.reveal, 0);
}

TEST_F(Render, SupersamplingSurvivesResizeAndPreservesFrozenComposition) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.beginFrame();
    ClearBackground(BLUE);
    DrawRectangle(600, 320, 80, 80, WHITE);
    renderer.present();
    EXPECT_GE(renderer.frameTexture().width, 2560);
    renderer.freezeFrame();
    renderer.startTransition(.25f);
    SetWindowSize(1920, 1080);
    PollInputEvents();
    renderer.updateTransition(1);
    renderer.beginFrame();
    renderer.drawFrozenFrame();
    renderer.present();
    EXPECT_EQ(renderer.frameTexture().width,
              Letterbox::renderSize(GetRenderWidth(), GetRenderHeight(), 2, 16384).width);
    EXPECT_EQ(renderer.frameTexture().width * 9, renderer.frameTexture().height * 16);
    auto frame = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&frame);
    EXPECT_EQ(GetImageColor(frame, 100, 300).b, 241);
    EXPECT_GT(GetImageColor(frame, 640, 360).r, 250);
    UnloadImage(frame);
    renderer.setReduceEffects(true);
    renderer.beginFrame();
    renderer.drawFrozenFrame();
    renderer.present();
    EXPECT_EQ(renderer.frameTexture().width,
              Letterbox::renderSize(GetRenderWidth(), GetRenderHeight(), 2, 16384).width);
}

TEST_F(Render, HaloHasSmoothCircularFalloffInsteadOfRevealingWholeSquareTiles) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    Level level;
    level.map = TileMap::parse(source, 48);
    Player player({640, 360}, PlayerConfig{});
    RippleSystem ripple(PingConfig{}, level.map);
    renderer.beginFrame();
    renderer.drawLevel(level, player, ripple, player.pos, 0, 1, false, 7);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    const auto ambient = GetImageColor(image, 1000, 429);
    const auto tileCorner = GetImageColor(image, 717, 429);
    EXPECT_NEAR(tileCorner.g, ambient.g, 2);
    EXPECT_GT(GetImageColor(image, 678, 398).g, ambient.g + 15);
    EXPECT_FLOAT_EQ(ripple.tileReveal(14, 8), 0);
    UnloadImage(image);
}
TEST_F(Render, SmoothHaloCannotIlluminateTheFloorThroughAWall) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    std::string rows;
    for (int y = 0; y < 15; ++y) {
        std::string row(40, '.');
        row[14] = '#';
        rows += row + '\n';
    }
    std::istringstream source(rows);
    Level level;
    level.map = TileMap::parse(source, 48);
    Player player({640, 360}, PlayerConfig{});
    RippleSystem ripple(PingConfig{}, level.map);
    renderer.beginFrame();
    renderer.drawLevel(level, player, ripple, player.pos, 0, 1, false, 7);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_NEAR(GetImageColor(image, 730, 375).g, GetImageColor(image, 1000, 375).g, 2);
    EXPECT_GT(GetImageColor(image, 640, 397).g, GetImageColor(image, 730, 375).g + 15);
    UnloadImage(image);
}

TEST_F(Render, PingLightingClipsTheCircleAndPartiallyVisibleTilesAtSubTileEdges) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    std::string rows;
    for (int y = 0; y < 15; ++y) rows += std::string(40, '.') + '\n';
    std::istringstream source(rows);
    Level level;
    level.map = TileMap::parse(source, 48);
    Player player({640, 360}, PlayerConfig{});
    RippleSystem ripple(PingConfig{}, level.map);
    ripple.startPing(player.pos, 0);
    std::vector<Entity*> entities;
    ripple.update(.2f, level.map, entities);
    const float untouchedTile = ripple.tileReveal(16, 9);
    EXPECT_FLOAT_EQ(untouchedTile, 0);
    renderer.beginFrame();
    renderer.drawLevel(level, player, ripple, player.pos, 0, 1, false, 7);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_NEAR(GetImageColor(image, 807, 361).g, GetImageColor(image, 1000, 361).g, 2);
    EXPECT_GT(GetImageColor(image, 770, 434).g, 60);
    EXPECT_FLOAT_EQ(ripple.tileReveal(16, 9), untouchedTile);
    UnloadImage(image);
}
TEST_F(Render, ResizingRetainsOutgoingTransitionAcrossAllPhysicalScissorRows) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.beginFrame();
    ClearBackground(Color{210, 20, 30, 255});
    renderer.present();
    renderer.startTransition(1);
    SetWindowSize(1500, 900);
    PollInputEvents();
    renderer.beginFrame();
    ClearBackground(BLUE);
    renderer.present();
    auto image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    for (int y : {100, 350, 600}) {
        EXPECT_EQ(GetImageColor(image, 640, y).r, 210);
        EXPECT_EQ(GetImageColor(image, 640, y).g, 20);
    }
    UnloadImage(image);
    renderer.updateTransition(.25f);
    renderer.beginFrame();
    ClearBackground(BLUE);
    renderer.present();
    image = logicalImage(renderer.frameTexture());
    ImageFlipVertical(&image);
    for (int y : {100, 350, 600}) {
        EXPECT_EQ(GetImageColor(image, 1100, y).r, 210);
        EXPECT_EQ(GetImageColor(image, 100, y).b, 241);
    }
    UnloadImage(image);
}

TEST_F(Render, CollectingTheKeycardDoesNotMakeItsDeskAppear) {
    std::ostringstream output;
    Logger logger(output, "");
    const auto config = Config::load("assets/config/tuning.json", logger);
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    ASSERT_EQ(level->map.tile(27, 22), TileType::Keycard);
    Player player(level->map.tileCenter({29, 23}), config.player);
    RippleSystem ripple(config.ping, level->map);
    Renderer renderer(logger, config.render);
    renderer.setReduceEffects(true);
    const auto desk = [&] {
        renderer.beginFrame();
        renderer.drawLevel(*level, player, ripple, player.pos, 0, 1, false, 7, {}, {}, {}, false,
                           nullptr, {}, true);
        renderer.present();
        auto image = logicalImage(renderer.frameTexture());
        ImageFlipVertical(&image);
        const auto surface = GetImageColor(image, 569, 293);
        UnloadImage(image);
        return surface;
    };
    const auto before = desk();
    level->map.removeKeycard(27, 22);
    const auto after = desk();
    EXPECT_EQ(before.r, after.r);
    EXPECT_EQ(before.g, after.g);
    EXPECT_EQ(before.b, after.b);
}
