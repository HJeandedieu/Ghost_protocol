#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/FirstPersonView.h"
#include "core/Logger.h"
#include "render/BankScene.h"
#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "systems/CombatSystem.h"
#include "systems/RippleSystem.h"
#include "world/World.h"

namespace {
World bankFixture() {
    Level level;
    level.name = "Perspective bank fixture";
    std::string rows;
    for (int y = 0; y < 10; ++y) {
        std::string row(10, '.');
        row[0] = row[9] = '#';
        row[5] = y == 4 ? 'd' : '#';
        if (y == 0 || y == 9) row.assign(10, '#');
        rows += row + '\n';
    }
    std::istringstream source(rows);
    level.map = TileMap::parse(source, 48);
    level.playerSpawn = {3, 4};
    return World(std::move(level), PlayerConfig{}, GuardConfig{}, CameraConfig{});
}

Image capture(Renderer& renderer, const World& world, const FirstPersonView& view,
              const RippleSystem& ripple) {
    renderer.beginFrame();
    renderer.drawPerspective(world, view, ripple, 1);
    renderer.present();
    auto image = LoadImageFromTexture(renderer.frameTexture());
    ImageResize(&image, 1280, 720);
    ImageFlipVertical(&image);
    return image;
}

int changedPixels(Image a, Image b, Rectangle region) {
    int changed = 0;
    for (int y = static_cast<int>(region.y); y < region.y + region.height; ++y)
        for (int x = static_cast<int>(region.x); x < region.x + region.width; ++x) {
            const auto first = GetImageColor(a, x, y), second = GetImageColor(b, x, y);
            if (first.r != second.r || first.g != second.g || first.b != second.b) ++changed;
        }
    return changed;
}
}  // namespace

class PerspectiveRender : public testing::Test {
   protected:
    void SetUp() override {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1280, 720, "Ghost Protocol perspective tests");
        ASSERT_TRUE(IsWindowReady());
    }
    void TearDown() override { CloseWindow(); }
};

TEST_F(PerspectiveRender, ClosedDoorOccludesActorsAndOpeningRevealsThemWithoutChangingReveal) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    world.alarmLoud = true;
    FirstPersonView view({});
    RippleSystem ripple({}, world.level.map);
    auto emptyClosed = capture(renderer, world, view, ripple);
    GuardSpawn spawn{"fixture-guard", "room", PatrolMode::Stationary, false, {{7, 4}}, 0};
    world.guards.emplace_back(spawn, world.level.map, GuardConfig{});
    auto actorClosed = capture(renderer, world, view, ripple);
    EXPECT_EQ(changedPixels(emptyClosed, actorClosed, {520, 180, 240, 400}), 0);
    const auto original = world.guards.front().pos;
    world.guards.front().pos = world.guards.front().prevPos = world.level.map.tileCenter({5, 4});
    auto insideClosed = capture(renderer, world, view, ripple);
    EXPECT_EQ(changedPixels(emptyClosed, insideClosed, {520, 180, 240, 400}), 0);
    world.guards.front().pos = world.guards.front().prevPos = original;
    world.level.map.setOpen(5, 4, true);
    auto actorOpen = capture(renderer, world, view, ripple);
    EXPECT_GT(changedPixels(actorClosed, actorOpen, {520, 180, 240, 400}), 1000);
    EXPECT_FLOAT_EQ(world.guards.front().reveal, 0);
    world.guards.clear();
    auto emptyOpen = capture(renderer, world, view, ripple);
    EXPECT_GT(changedPixels(actorOpen, emptyOpen, {520, 180, 240, 400}), 100);
    EXPECT_FLOAT_EQ(ripple.tileReveal(5, 4), 0);
    EXPECT_TRUE(world.level.map.isOpen(5, 4));
    UnloadImage(emptyClosed);
    UnloadImage(actorClosed);
    UnloadImage(insideClosed);
    UnloadImage(actorOpen);
    UnloadImage(emptyOpen);
    EXPECT_EQ(output.str().find("[ERROR]"), std::string::npos);
}

TEST_F(PerspectiveRender, PhaseCameraAndCrouchChangePerspectiveWithoutMutatingTheMap) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    FirstPersonView view({});
    RippleSystem ripple({}, world.level.map);
    auto stealth = capture(renderer, world, view, ripple);
    world.alarmLoud = true;
    auto loud = capture(renderer, world, view, ripple);
    const auto stealthWall = GetImageColor(stealth, 640, 300);
    const auto loudWall = GetImageColor(loud, 640, 300);
    EXPECT_GT(stealthWall.g, stealthWall.r);
    EXPECT_GT(loudWall.r, loudWall.g);
    EXPECT_GT(changedPixels(stealth, loud, {0, 100, 1280, 620}), 10000);
    view.look({900, 0});
    auto turned = capture(renderer, world, view, ripple);
    EXPECT_GT(changedPixels(loud, turned, {0, 100, 1280, 620}), 10000);
    view.look({0, 200});
    auto pitched = capture(renderer, world, view, ripple);
    EXPECT_GT(changedPixels(turned, pitched, {0, 100, 1280, 620}), 10000);
    Input input;
    input.crouchPressed = true;
    world.player.update(1.f / 60, input, world.level.map);
    auto crouched = capture(renderer, world, view, ripple);
    EXPECT_GT(changedPixels(pitched, crouched, {0, 100, 1280, 620}), 10000);
    EXPECT_EQ(world.level.map.tile(5, 3), TileType::Wall);
    EXPECT_FALSE(world.level.map.isOpen(5, 4));
    EXPECT_FLOAT_EQ(ripple.tileReveal(3, 4), 0);
    EXPECT_TRUE(
        ExportImage(stealth, GP_RENDER_OUTPUT_DIRECTORY "/perspective-stealth-fixture.png"));
    EXPECT_TRUE(ExportImage(loud, GP_RENDER_OUTPUT_DIRECTORY "/perspective-loud-fixture.png"));
    UnloadImage(stealth);
    UnloadImage(loud);
    UnloadImage(turned);
    UnloadImage(pitched);
    UnloadImage(crouched);
}

TEST_F(PerspectiveRender, ArchitectureCacheSurvivesDoorChangesAndRebuildsForANewLevel) {
    std::ostringstream output;
    Logger logger(output, "");
    BankScene scene(logger);
    auto first = bankFixture();
    RippleSystem ripple({}, first.level.map);
    const ViewConfig view;
    const RenderConfig render;
    scene.draw(first, ripple, view, render, 1, 0, nullptr);
    EXPECT_EQ(scene.geometryRevision(), 1);
    first.level.map.setOpen(5, 4, true);
    scene.draw(first, ripple, view, render, 1, 1, nullptr);
    EXPECT_EQ(scene.geometryRevision(), 1);
    auto second = bankFixture();
    RippleSystem otherRipple({}, second.level.map);
    scene.draw(second, otherRipple, view, render, 1, 0, nullptr);
    EXPECT_EQ(scene.geometryRevision(), 2);
}

TEST_F(PerspectiveRender, PitchedGeometryImpactAppearsAtTheFixedCenterReticle) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    world.alarmLoud = true;
    FirstPersonView view({});
    view.look({0, 80});
    RippleSystem ripple({}, world.level.map);
    auto before = capture(renderer, world, view, ripple);
    EventBus events;
    auto weapons = loadWeapons("assets/config/weapons.json", logger).value();
    for (auto& spec : weapons) spec.spreadDeg = 0;
    CombatSystem combat(events, weapons, 42);
    Input input;
    input.firePressed = true;
    input.mouseInViewport = true;
    combat.update(1.f / 60, input, view.shotRay(world.player.pos, false), world);
    ASSERT_EQ(combat.lastShot().pellets.size(), 1u);
    EXPECT_EQ(combat.lastShot().pellets[0].impact, ShotImpact::Geometry);
    EXPECT_LT(combat.lastShot().pellets[0].to3D.y, ViewConfig{}.eyeHeight);
    renderer.beginFrame();
    renderer.drawPerspective(world, view, ripple, 1, nullptr, nullptr, &combat);
    renderer.present();
    auto after = LoadImageFromTexture(renderer.frameTexture());
    ImageResize(&after, 1280, 720);
    ImageFlipVertical(&after);
    EXPECT_GT(changedPixels(before, after, {630, 350, 20, 20}), 4);
    EXPECT_TRUE(ExportImage(after, GP_RENDER_OUTPUT_DIRECTORY "/perspective-pitched-impact.png"));
    UnloadImage(before);
    UnloadImage(after);
}

TEST_F(PerspectiveRender, PhysicalResizePreservesPerspectiveAndFrozenFrame) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    world.alarmLoud = true;
    FirstPersonView view({});
    RippleSystem ripple({}, world.level.map);
    auto before = capture(renderer, world, view, ripple);
    renderer.freezeFrame();
    SetWindowSize(1920, 1080);
    PollInputEvents();
    renderer.beginFrame();
    renderer.drawFrozenFrame();
    renderer.present();
    auto frozen = LoadImageFromTexture(renderer.frameTexture());
    ImageResize(&frozen, 1280, 720);
    ImageFlipVertical(&frozen);
    const auto first = GetImageColor(before, 640, 300), copied = GetImageColor(frozen, 640, 300);
    EXPECT_NEAR(first.r, copied.r, 3);
    EXPECT_NEAR(first.g, copied.g, 3);
    auto after = capture(renderer, world, view, ripple);
    const auto resized = GetImageColor(after, 640, 300);
    EXPECT_NEAR(first.r, resized.r, 3);
    EXPECT_NEAR(first.g, resized.g, 3);
    EXPECT_EQ(renderer.frameTexture().width,
              Letterbox::renderSize(GetRenderWidth(), GetRenderHeight(), 2, 16384).width);
    UnloadImage(before);
    UnloadImage(frozen);
    UnloadImage(after);
}
