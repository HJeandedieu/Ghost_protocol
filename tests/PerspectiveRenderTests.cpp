#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/FirstPersonView.h"
#include "core/Logger.h"
#include "render/BankScene.h"
#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "render/TacticalArt.h"
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

TEST_F(PerspectiveRender, ForegroundWeaponsSwitchAndReloadWithoutMovingAimOrWritingWorldState) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    world.alarmLoud = true;
    FirstPersonView view({});
    RippleSystem ripple({}, world.level.map);
    EventBus events;
    const auto specs = loadWeapons("assets/config/weapons.json", logger);
    ASSERT_TRUE(specs);
    CombatSystem combat(events, *specs, 42);
    const auto render = [&] {
        renderer.beginFrame();
        renderer.drawPerspective(world, view, ripple, 1, nullptr, nullptr, &combat);
        renderer.present();
        auto result = LoadImageFromTexture(renderer.frameTexture());
        ImageResize(&result, 1280, 720);
        ImageFlipVertical(&result);
        return result;
    };
    const auto position = world.player.pos;
    auto pistol = render();
    Input input;
    input.weaponSlot = 1;
    combat.update(1.f / 60, input, view.shotRay(position, false), world);
    auto smg = render();
    EXPECT_GT(changedPixels(pistol, smg, {720, 440, 520, 280}), 1000);
    EXPECT_EQ(changedPixels(pistol, smg, {625, 345, 30, 30}), 0);
    input = {};
    input.mouseInViewport = true;
    input.firePressed = true;
    combat.update(.1f, input, view.shotRay(position, false), world);
    input.firePressed = false;
    input.reloadPressed = true;
    combat.update(.1f, input, view.shotRay(position, false), world);
    input.reloadPressed = false;
    combat.update(combat.activeWeapon().spec().reload * .5f, input, view.shotRay(position, false),
                  world);
    auto reload = render();
    EXPECT_GT(changedPixels(smg, reload, {720, 440, 520, 280}), 1000);
    EXPECT_EQ(changedPixels(smg, reload, {625, 345, 30, 30}), 0);
    EXPECT_FLOAT_EQ(world.player.pos.x, position.x);
    EXPECT_FLOAT_EQ(world.player.pos.y, position.y);
    EXPECT_FLOAT_EQ(view.pitchDeg(), 0);
    EXPECT_FALSE(world.level.map.isOpen(5, 4));
    EXPECT_EQ(output.str().find("[ERROR]"), std::string::npos);
    UnloadImage(pistol);
    UnloadImage(smg);
    UnloadImage(reload);
}

TEST_F(PerspectiveRender, WeaponDepthIsIndependentOfNearbyClosedDoorDepth) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    world.alarmLoud = true;
    FirstPersonView view({});
    RippleSystem ripple({}, world.level.map);
    EventBus events;
    const auto specs = loadWeapons("assets/config/weapons.json", logger);
    ASSERT_TRUE(specs);
    CombatSystem combat(events, *specs, 42);
    const auto frame = [&](bool weapon) {
        renderer.beginFrame();
        renderer.drawPerspective(world, view, ripple, 1, nullptr, nullptr,
                                 weapon ? &combat : nullptr);
        renderer.present();
        auto result = LoadImageFromTexture(renderer.frameTexture());
        ImageResize(&result, 1280, 720);
        ImageFlipVertical(&result);
        return result;
    };
    auto farBase = frame(false), farGun = frame(true);
    world.player.pos = world.player.prevPos = {239, 216};
    auto nearBase = frame(false), nearGun = frame(true);
    int stableWeaponPixels = 0;
    for (int y = 440; y < 720; ++y)
        for (int x = 720; x < 1240; ++x) {
            const auto a = GetImageColor(farGun, x, y), b = GetImageColor(nearGun, x, y);
            const auto c = GetImageColor(farBase, x, y), d = GetImageColor(nearBase, x, y);
            const bool weaponPixel = a.r != c.r || a.g != c.g || a.b != c.b;
            const bool nearWeapon = b.r != d.r || b.g != d.g || b.b != d.b;
            if (weaponPixel && nearWeapon && a.r == b.r && a.g == b.g && a.b == b.b)
                ++stableWeaponPixels;
        }
    EXPECT_GT(stableWeaponPixels, 10000);
    EXPECT_FALSE(world.level.map.isOpen(5, 4));
    UnloadImage(farBase);
    UnloadImage(farGun);
    UnloadImage(nearBase);
    UnloadImage(nearGun);
}

TEST_F(PerspectiveRender, DetailedActorRemainsHiddenUntilRippleRevealsIt) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    world.level.map.setOpen(5, 4, true);
    FirstPersonView view({});
    RippleSystem ripple({}, world.level.map);
    auto empty = capture(renderer, world, view, ripple);
    GuardSpawn spawn{"hidden", "room", PatrolMode::Stationary, false, {{7, 4}}, 180};
    world.guards.emplace_back(spawn, world.level.map, GuardConfig{});
    auto hidden = capture(renderer, world, view, ripple);
    EXPECT_EQ(changedPixels(empty, hidden, {500, 180, 280, 420}), 0);
    ripple.startPing(world.player.pos, PingConfig{}.chargeMax);
    std::vector<Entity*> entities{&world.guards.front()};
    ripple.update(.8f, world.level.map, entities);
    ASSERT_GT(world.guards.front().reveal, 0);
    const float reveal = world.guards.front().reveal;
    auto visible = capture(renderer, world, view, ripple);
    EXPECT_FLOAT_EQ(world.guards.front().reveal, reveal);
    world.guards.clear();
    auto revealedEmpty = capture(renderer, world, view, ripple);
    EXPECT_GT(changedPixels(visible, revealedEmpty, {500, 180, 280, 420}), 1000);
    UnloadImage(empty);
    UnloadImage(hidden);
    UnloadImage(visible);
    UnloadImage(revealedEmpty);
}
TEST_F(PerspectiveRender, CameraConeIsDepthOccludedAndSecurityLoopSuppressesIt) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    auto world = bankFixture();
    FirstPersonView view({});
    view.look({0, 160});
    RippleSystem ripple({}, world.level.map);
    CameraSpawn spawn{"camera", "room", {7, 4}, 180, 0, 180};
    world.cameras.emplace_back(spawn, world.level.map, CameraConfig{});
    ripple.startPing(world.player.pos, PingConfig{}.chargeMax);
    std::vector<Entity*> entities{&world.cameras.front()};
    ripple.update(.8f, world.level.map, entities);
    world.securityLoopRemaining = 100;
    auto closedDisabled = capture(renderer, world, view, ripple);
    world.securityLoopRemaining = 0;
    auto closedActive = capture(renderer, world, view, ripple);
    EXPECT_EQ(changedPixels(closedDisabled, closedActive, {460, 400, 360, 280}), 0);
    world.level.map.setOpen(5, 4, true);
    world.level.map.fillLight(LightLevel::Lit);
    auto active = capture(renderer, world, view, ripple);
    world.securityLoopRemaining = 100;
    auto disabled = capture(renderer, world, view, ripple);
    EXPECT_GT(changedPixels(active, disabled, {460, 400, 360, 280}), 100);
    EXPECT_TRUE(world.level.map.isOpen(5, 4));
    UnloadImage(closedDisabled);
    UnloadImage(closedActive);
    UnloadImage(active);
    UnloadImage(disabled);
}
