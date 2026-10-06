#include <gtest/gtest.h>

#include <sstream>

#include "entities/Entity.h"
#include "systems/RippleSystem.h"
#include "world/TileMap.h"

namespace {
TileMap openMap() {
    std::istringstream source(std::string(30, '.') + '\n' + std::string(30, '.') + '\n' +
                              std::string(30, '.'));
    return TileMap::parse(source, 48);
}
}  // namespace

TEST(RippleSystem, TapChargeAndHoldClampToDocumentedRadii) {
    const auto map = openMap();
    for (const auto& pair :
         {std::pair<float, float>{0.1f, 260}, {0.25f, 260}, {0.525f, 390}, {0.8f, 520}, {2, 520}}) {
        RippleSystem ripple(PingConfig{}, map);
        ripple.startPing({24, 72}, pair.first);
        EXPECT_NEAR(ripple.maxRadius(), pair.second, 0.001f);
        EXPECT_FLOAT_EQ(ripple.cooldownRemaining(), 3);
    }
}

TEST(RippleSystem, WaveExpandsAtConfiguredSpeedAndOnlyRevealsCrossedTiles) {
    const auto map = openMap();
    RippleSystem ripple(PingConfig{}, map);
    std::vector<Entity*> entities;
    ripple.startPing({24, 72}, 0);
    ripple.update(0.1f, map, entities);
    EXPECT_FLOAT_EQ(ripple.waveRadius(), 80);
    EXPECT_GT(ripple.tileReveal(1, 1), 0.9f);
    EXPECT_FLOAT_EQ(ripple.tileReveal(2, 1), 0);
    ripple.update(0.1f, map, entities);
    EXPECT_GT(ripple.tileReveal(2, 1), 0.9f);
    EXPECT_FLOAT_EQ(ripple.tileReveal(6, 1), 0);
}

TEST(RippleSystem, RevealsBlockingWallButNotTheFloorOrEntityBehindIt) {
    std::istringstream source("......\n..#...\n......");
    const auto map = TileMap::parse(source, 48);
    RippleSystem ripple(PingConfig{}, map);
    Entity visible, hidden;
    visible.pos = {72, 72};
    hidden.pos = {168, 72};
    std::vector<Entity*> entities{&visible, &hidden};
    ripple.startPing({24, 72}, 0);
    ripple.update(0.3f, map, entities);
    EXPECT_GT(ripple.tileReveal(2, 1), 0);
    EXPECT_FLOAT_EQ(ripple.tileReveal(3, 1), 0);
    EXPECT_GT(visible.reveal, 0);
    EXPECT_FLOAT_EQ(hidden.reveal, 0);
    EXPECT_FLOAT_EQ(ripple.visibility(3, 1, {72, 72}, map), 0);
}

TEST(RippleSystem, RevealDecaysAndCooldownRejectsExtraPings) {
    const auto map = openMap();
    RippleSystem ripple(PingConfig{}, map);
    std::vector<Entity*> entities;
    ripple.startPing({24, 72}, 0);
    ripple.update(0.06f, map, entities);
    EXPECT_FLOAT_EQ(ripple.tileReveal(1, 1), 1);
    ripple.startPing({120, 72}, 0.8f);
    EXPECT_FLOAT_EQ(ripple.origin().x, 24);
    EXPECT_FLOAT_EQ(ripple.maxRadius(), 260);
    ripple.update(1.25f, map, entities);
    EXPECT_NEAR(ripple.tileReveal(1, 1), 0.5f, 0.001f);
    ripple.update(1.7f, map, entities);
    EXPECT_FLOAT_EQ(ripple.tileReveal(1, 1), 0);
    EXPECT_FLOAT_EQ(ripple.cooldownRemaining(), 0);
    EXPECT_FLOAT_EQ(ripple.cooldownFraction(), 1);
    ripple.startPing({120, 72}, 0.8f);
    EXPECT_FLOAT_EQ(ripple.maxRadius(), 520);
}

TEST(RippleSystem, ReleaseFiresChargeAndShortBetweenFrameTapIsRetained) {
    const auto map = openMap();
    RippleSystem charged(PingConfig{}, map);
    charged.updateCharge(0.4f, true, true, {24, 72});
    EXPECT_TRUE(charged.charging());
    EXPECT_FALSE(charged.waveActive());
    charged.updateCharge(0.4f, true, false, {24, 72});
    charged.updateCharge(0.01f, false, false, {24, 72});
    EXPECT_FALSE(charged.charging());
    EXPECT_FLOAT_EQ(charged.maxRadius(), 520);
    RippleSystem tap(PingConfig{}, map);
    tap.updateCharge(1.0f / 60, false, true, {24, 72});
    EXPECT_TRUE(tap.waveActive());
    EXPECT_FLOAT_EQ(tap.maxRadius(), 260);
}

TEST(RippleSystem, LightingHaloAndRevealAreCombinedWithoutChangingStoredReveal) {
    auto map = openMap();
    map.setLight(10, 1, LightLevel::Lit);
    map.setLight(11, 1, LightLevel::Dim);
    RippleSystem ripple(PingConfig{}, map);
    EXPECT_FLOAT_EQ(ripple.visibility(10, 1, {24, 72}, map), 1);
    EXPECT_FLOAT_EQ(ripple.visibility(11, 1, {24, 72}, map), 0.35f);
    EXPECT_FLOAT_EQ(ripple.visibility(12, 1, {24, 72}, map), 0);
    EXPECT_FLOAT_EQ(ripple.visibility(1, 1, {24, 72}, map), 1);
    EXPECT_FLOAT_EQ(ripple.tileReveal(1, 1), 0);
    EXPECT_FLOAT_EQ(ripple.visibility(-1, 0, {24, 72}, map), 0);
}

TEST(RippleSystem, CustomTuningAndInvalidTicksAreRespected) {
    const auto map = openMap();
    PingConfig config;
    config.smallRadius = 100;
    config.bigRadius = 200;
    config.speed = 400;
    config.cooldown = 2;
    RippleSystem ripple(config, map);
    std::vector<Entity*> entities;
    ripple.startPing({24, 72}, 0.8f);
    ripple.update(-1, map, entities);
    EXPECT_FLOAT_EQ(ripple.waveRadius(), 0);
    ripple.update(0.1f, map, entities);
    EXPECT_FLOAT_EQ(ripple.waveRadius(), 40);
    EXPECT_FLOAT_EQ(ripple.maxRadius(), 200);
    EXPECT_NEAR(ripple.cooldownRemaining(), 1.9f, 0.001f);
}

TEST(RippleSystem, LargeTickAgesNewRevealsFromTheirActualArrivalTime) {
    const auto map = openMap();
    RippleSystem whole(PingConfig{}, map), split(PingConfig{}, map);
    Entity entityWhole, entitySplit;
    entityWhole.pos = entitySplit.pos = {216, 72};
    std::vector<Entity*> wholeEntities{&entityWhole}, splitEntities{&entitySplit};
    whole.startPing({24, 72}, 0);
    split.startPing({24, 72}, 0);
    whole.update(1, map, wholeEntities);
    for (int i = 0; i < 100; ++i) split.update(0.01f, map, splitEntities);
    EXPECT_NEAR(whole.tileReveal(4, 1), split.tileReveal(4, 1), 0.001f);
    EXPECT_NEAR(entityWhole.reveal, entitySplit.reveal, 0.001f);
}
