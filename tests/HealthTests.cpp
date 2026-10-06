#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Player.h"
#include "render/HealthHud.h"
#include "systems/CombatSystem.h"
class HealthTest : public testing::Test {
   protected:
    std::ostringstream console;
    Logger logger{console, ""};
    EventBus bus;
    std::vector<WeaponSpec> weapons = loadWeapons("assets/config/weapons.json", logger).value();
    CombatSystem combat{bus, weapons, 42};
    Player player{{100, 100}, PlayerConfig{}};
};
TEST_F(HealthTest, ArmorAbsorbsFirstAndOverflowReducesHpWithQueuedDamageEvent) {
    int events = 0;
    bus.subscribe<EntityDamaged>([&](const auto& event) {
        EXPECT_EQ(event.targetId, "Ghost");
        EXPECT_EQ(event.sourceId, "cop");
        EXPECT_FLOAT_EQ(event.amount, 65);
        ++events;
    });
    combat.applyDamage(player, 65, "cop");
    EXPECT_FLOAT_EQ(player.armor(), 0);
    EXPECT_FLOAT_EQ(player.hp(), 85);
    EXPECT_EQ(events, 0);
    bus.dispatch();
    EXPECT_EQ(events, 1);
}
TEST_F(HealthTest, RegenerationStartsOnlyAfterFiveSecondsAndCountsOnlyPostDelayTime) {
    combat.applyDamage(player, 50, "cop");
    combat.updateHealth(4.5f, player);
    EXPECT_FLOAT_EQ(player.armor(), 0);
    combat.updateHealth(1, player);
    EXPECT_FLOAT_EQ(player.armor(), 4);
    combat.updateHealth(1, player);
    EXPECT_FLOAT_EQ(player.armor(), 12);
    combat.updateHealth(100, player);
    EXPECT_FLOAT_EQ(player.armor(), 50);
    EXPECT_FLOAT_EQ(player.hp(), 100);
}
TEST_F(HealthTest, NewDamageResetsDelayAndHpNeverRegenerates) {
    combat.applyDamage(player, 70, "cop");
    combat.updateHealth(6, player);
    EXPECT_FLOAT_EQ(player.armor(), 8);
    combat.applyDamage(player, 4, "cop");
    combat.updateHealth(5, player);
    EXPECT_FLOAT_EQ(player.armor(), 4);
    combat.updateHealth(0.25f, player);
    EXPECT_FLOAT_EQ(player.armor(), 6);
    EXPECT_FLOAT_EQ(player.hp(), 80);
}
TEST_F(HealthTest, FixedTicksRespectDelayAndCustomConfig) {
    PlayerConfig config;
    config.hp = 70;
    config.armor = 20;
    config.armorRegen = 4;
    config.armorRegenDelay = 2;
    Player custom({0, 0}, config);
    combat.applyDamage(custom, 20, "cop");
    for (int tick = 0; tick < 120; ++tick) combat.updateHealth(1.0f / 60, custom);
    EXPECT_NEAR(custom.armor(), 0, 0.0001f);
    for (int tick = 0; tick < 60; ++tick) combat.updateHealth(1.0f / 60, custom);
    EXPECT_NEAR(custom.armor(), 4, 0.0001f);
    EXPECT_FLOAT_EQ(custom.hp(), 70);
}
TEST_F(HealthTest, InvalidDamageAndTimeDoNotMutateVitalsOrEmitEvents) {
    int events = 0;
    bus.subscribe<EntityDamaged>([&](const auto&) { ++events; });
    for (float damage : {0.0f, -10.0f, std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()})
        combat.applyDamage(player, damage, "bad");
    bus.dispatch();
    EXPECT_EQ(events, 0);
    EXPECT_FLOAT_EQ(player.hp(), 100);
    EXPECT_FLOAT_EQ(player.armor(), 50);
    combat.applyDamage(player, 50, "cop");
    combat.updateHealth(-1, player);
    combat.updateHealth(std::numeric_limits<float>::infinity(), player);
    combat.updateHealth(5, player);
    EXPECT_FLOAT_EQ(player.armor(), 0);
}
TEST_F(HealthTest, LethalDamageClampsAndPublishesDeathAndDownedOnceWithoutRevival) {
    int deaths = 0, downed = 0, damage = 0;
    bus.subscribe<EntityDied>([&](const auto& event) {
        EXPECT_EQ(event.targetId, "Ghost");
        ++deaths;
    });
    bus.subscribe<PlayerDowned>([&](const auto&) { ++downed; });
    bus.subscribe<EntityDamaged>([&](const auto&) { ++damage; });
    combat.applyDamage(player, 500, "cop");
    combat.applyDamage(player, 50, "cop");
    combat.updateHealth(100, player);
    bus.dispatch();
    EXPECT_TRUE(player.dead());
    EXPECT_FLOAT_EQ(player.hp(), 0);
    EXPECT_FLOAT_EQ(player.armor(), 0);
    EXPECT_EQ(deaths, 1);
    EXPECT_EQ(downed, 1);
    EXPECT_EQ(damage, 1);
}
TEST_F(HealthTest, HudDrainsOverPointThreeSecondsAndReadsWithoutChangingHealth) {
    HealthHud hud;
    hud.update(0, player);
    bus.subscribe<EntityDamaged>([&](const auto&) { hud.damaged(); });
    combat.applyDamage(player, 75, "cop");
    bus.dispatch();
    hud.update(0.15f, player);
    EXPECT_FLOAT_EQ(hud.displayedHp(), 87.5f);
    EXPECT_FLOAT_EQ(hud.displayedArmor(), 25);
    EXPECT_NEAR(hud.flashFraction(), 0.5f, 0.0001f);
    EXPECT_FLOAT_EQ(player.hp(), 75);
    EXPECT_FLOAT_EQ(player.armor(), 0);
    hud.update(0.15f, player);
    EXPECT_FLOAT_EQ(hud.displayedHp(), 75);
    EXPECT_FLOAT_EQ(hud.displayedArmor(), 0);
    EXPECT_FLOAT_EQ(hud.flashFraction(), 0);
    hud.update(-1, player);
    EXPECT_FLOAT_EQ(hud.displayedHp(), 75);
}
