#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "render/HealthHud.h"
#include "systems/CombatSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/PickupSystem.h"
#include "systems/RippleSystem.h"
#include "world/World.h"

namespace {
World pickupWorld(const std::string& tiles = "........\n........\n........\n") {
    Level level;
    std::istringstream text(tiles);
    level.map = TileMap::parse(text, 48);
    level.playerSpawn = {1, 1};
    return World(std::move(level), PlayerConfig{});
}
}  // namespace
class PickupTest : public testing::Test {
   protected:
    std::ostringstream console;
    Logger logger{console, ""};
    EventBus bus;
    CombatSystem combat{bus, loadWeapons("assets/config/weapons.json", logger).value(), 42};
    PickupSystem pickups{bus, PickupConfig{}, 42};
    World world = pickupWorld();
};

TEST_F(PickupTest, SeededPoliceRollsMatchMutuallyExclusiveOutcomesAndExcludeGuards) {
    Rng expected(42);
    int medkits = 0, plates = 0, none = 0;
    pickups.dropForPolice("G01", "patrol_guard", world.player.pos, world);
    EXPECT_TRUE(world.pickups.empty());
    for (int i = 0; i < 100; ++i) {
        const float roll = expected.uniformFloat(0, 1);
        const auto before = world.pickups.size();
        const std::string type = i % 3 == 0 ? "cop" : (i % 3 == 1 ? "shield_cop" : "heavy");
        const auto id = "police:" + std::to_string(i);
        pickups.dropForPolice(id, type, {100, 100}, world);
        if (roll < 0.3f) {
            ASSERT_EQ(world.pickups.size(), before + 1);
            EXPECT_EQ(world.pickups.back()->type,
                      roll < 0.2f ? PickupType::Medkit : PickupType::ArmorPlate);
            EXPECT_FLOAT_EQ(world.pickups.back()->pos.x, 100);
            if (roll < 0.2f)
                ++medkits;
            else
                ++plates;
        } else {
            EXPECT_EQ(world.pickups.size(), before);
            ++none;
        }
        const auto after = world.pickups.size();
        pickups.dropForPolice(id, type, {100, 100}, world);
        EXPECT_EQ(world.pickups.size(), after);
    }
    EXPECT_GT(medkits, 0);
    EXPECT_GT(plates, 0);
    EXPECT_GT(none, 0);
}
TEST_F(PickupTest, ERestoresCapsConsumesOnlyOnceAndPublishesCollectionWithoutDamage) {
    combat.applyDamage(world.player, 110, "cop");
    bus.dispatch();
    int damaged = 0, collected = 0;
    bus.subscribe<EntityDamaged>([&](const auto&) { ++damaged; });
    bus.subscribe<InteractionDone>([&](const auto& event) {
        EXPECT_EQ(event.interactableId, "pickup:0");
        ++collected;
    });
    pickups.spawn(PickupType::Medkit, world.player.pos, world);
    pickups.update(false, false, world, combat);
    EXPECT_FLOAT_EQ(world.player.hp(), 40);
    pickups.update(true, false, world, combat);
    EXPECT_FLOAT_EQ(world.player.hp(), 90);
    pickups.update(true, false, world, combat);
    bus.dispatch();
    EXPECT_TRUE(world.pickups.empty());
    EXPECT_EQ(collected, 1);
    EXPECT_EQ(damaged, 0);
    pickups.spawn(PickupType::Medkit, world.player.pos, world);
    pickups.spawn(PickupType::ArmorPlate, world.player.pos, world);
    pickups.update(true, false, world, combat);
    EXPECT_FLOAT_EQ(world.player.hp(), 100);
    EXPECT_EQ(world.pickups.size(), 1u);
    pickups.update(true, false, world, combat);
    EXPECT_FLOAT_EQ(world.player.armor(), 50);
}
TEST_F(PickupTest, FullResourcesRemainAndNearestEligibleWinsWithStableTies) {
    pickups.spawn(PickupType::Medkit, world.player.pos, world);
    pickups.spawn(PickupType::ArmorPlate, {world.player.pos.x + 20, world.player.pos.y}, world);
    pickups.spawn(PickupType::ArmorPlate, {world.player.pos.x - 20, world.player.pos.y}, world);
    pickups.update(true, false, world, combat);
    EXPECT_EQ(world.pickups.size(), 3u);
    combat.applyDamage(world.player, 10, "cop");
    ASSERT_NE(pickups.target(world, false), nullptr);
    EXPECT_EQ(pickups.target(world, false)->id, "pickup:1");
    pickups.update(true, false, world, combat);
    EXPECT_EQ(world.pickups.size(), 2u);
    EXPECT_FLOAT_EQ(world.player.armor(), 50);
}
TEST_F(PickupTest, RangeBoundaryClearSightAndClosedWallsGateCollection) {
    combat.applyDamage(world.player, 50, "cop");
    pickups.spawn(PickupType::ArmorPlate, {world.player.pos.x + 50.01f, world.player.pos.y}, world);
    EXPECT_EQ(pickups.target(world, false), nullptr);
    world.pickups.front()->pos.x = world.player.pos.x + 50;
    EXPECT_NE(pickups.target(world, false), nullptr);
    auto blockedWorld = pickupWorld("........\n..S.....\n........\n");
    blockedWorld.player.pos = {95, 72};
    combat.applyDamage(blockedWorld.player, 50, "cop");
    pickups.spawn(PickupType::ArmorPlate, {145, 72}, blockedWorld);
    EXPECT_EQ(pickups.target(blockedWorld, false), nullptr);
    blockedWorld.level.map.setOpen(2, 1, true);
    EXPECT_NE(pickups.target(blockedWorld, false), nullptr);
}
TEST_F(PickupTest, CompletingMissionOrPagerInteractionStillClaimsTheTick) {
    combat.applyDamage(world.player, 50, "cop");
    pickups.spawn(PickupType::ArmorPlate, world.player.pos, world);
    InteractionSystem interaction(bus);
    bool available = true;
    interaction.add({"pager", world.player.pos, 0, "E: answer pager",
                     [&](const Player&) { return available; }, [&](World&) { available = false; }});
    interaction.update(1.0f / 60, true, world);
    EXPECT_EQ(interaction.target(), nullptr);
    EXPECT_TRUE(interaction.claimedThisTick());
    pickups.update(true, interaction.claimedThisTick(), world, combat);
    EXPECT_FLOAT_EQ(world.player.armor(), 0);
    EXPECT_EQ(world.pickups.size(), 1u);
    interaction.update(1.0f / 60, false, world);
    pickups.update(true, interaction.claimedThisTick(), world, combat);
    EXPECT_FLOAT_EQ(world.player.armor(), 50);
}
TEST_F(PickupTest, DownedPlayerAndInvalidRestorationCannotReviveOrConsume) {
    combat.applyDamage(world.player, 200, "cop");
    pickups.spawn(PickupType::Medkit, world.player.pos, world);
    pickups.update(true, false, world, combat);
    EXPECT_TRUE(world.player.dead());
    EXPECT_EQ(world.pickups.size(), 1u);
    EXPECT_FLOAT_EQ(combat.restore(world.player, PickupType::Medkit, 50), 0);
    Player alive({0, 0}, PlayerConfig{});
    combat.applyDamage(alive, 100, "cop");
    for (float amount : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()})
        EXPECT_FLOAT_EQ(combat.restore(alive, PickupType::Medkit, amount), 0);
    EXPECT_FLOAT_EQ(alive.hp(), 50);
}
TEST_F(PickupTest, HealingDoesNotResetRegenDelayOrProduceHudHitFlash) {
    HealthHud hud;
    combat.applyDamage(world.player, 75, "cop");
    hud.update(0, world.player);
    combat.updateHealth(4, world.player);
    EXPECT_FLOAT_EQ(combat.restore(world.player, PickupType::ArmorPlate, 10), 10);
    combat.updateHealth(2, world.player);
    EXPECT_FLOAT_EQ(world.player.armor(), 18);
    combat.restore(world.player, PickupType::Medkit, 50);
    hud.update(0.1f, world.player);
    EXPECT_FLOAT_EQ(hud.displayedHp(), 100);
    EXPECT_FLOAT_EQ(hud.flashFraction(), 0);
}
TEST_F(PickupTest, RippleRevealsDropsAndTheyPersistUntilCollectedOrWorldReloaded) {
    pickups.spawn(PickupType::Medkit, {100, 72}, world);
    RippleSystem ripple(PingConfig{}, world.level.map);
    std::vector<Entity*> entities{world.pickups.front().get()};
    EXPECT_FLOAT_EQ(world.pickups.front()->reveal, 0);
    ripple.startPing(world.player.pos, 0);
    ripple.update(0.1f, world.level.map, entities);
    EXPECT_GT(world.pickups.front()->reveal, 0);
    ripple.update(3, world.level.map, entities);
    EXPECT_EQ(world.pickups.size(), 1u);
    EXPECT_FLOAT_EQ(world.pickups.front()->reveal, 0);
    auto reloadedWorld = pickupWorld();
    EXPECT_TRUE(reloadedWorld.pickups.empty());
}

TEST_F(PickupTest, CustomTuningControlsGuaranteedDropAndRestoration) {
    PickupConfig config;
    config.medkitChance = 0;
    config.armorChance = 1;
    config.armorAmount = 15;
    config.collectRadius = 10;
    PickupSystem custom(bus, config, 123);
    combat.applyDamage(world.player, 50, "cop");
    custom.dropForPolice("C01", "cop", {world.player.pos.x + 11, world.player.pos.y}, world);
    ASSERT_EQ(world.pickups.size(), 1u);
    EXPECT_EQ(world.pickups.front()->type, PickupType::ArmorPlate);
    EXPECT_EQ(custom.target(world, false), nullptr);
    world.pickups.front()->pos = world.player.pos;
    custom.update(true, false, world, combat);
    EXPECT_FLOAT_EQ(world.player.armor(), 15);
    config.armorChance = 0;
    PickupSystem noDrops(bus, config, 123);
    noDrops.dropForPolice("C02", "cop", world.player.pos, world);
    EXPECT_TRUE(world.pickups.empty());
}
