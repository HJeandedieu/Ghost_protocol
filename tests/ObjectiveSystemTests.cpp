#include <gtest/gtest.h>

#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "systems/AlarmDirector.h"
#include "systems/InteractionSystem.h"
#include "systems/ObjectiveSystem.h"
#include "world/LevelLoader.h"
#include "world/World.h"

namespace {
Level level(const std::string& text) {
    Level result;
    std::istringstream source(text);
    result.map = TileMap::parse(source, 48);
    result.playerSpawn = {1, 1};
    return result;
}
struct Mission {
    std::ostringstream output;
    Logger logger{output, ""};
    Config config;
    EventBus events;
    World world;
    InteractionSystem interaction{events};
    AlarmDirector alarm;
    std::unique_ptr<ObjectiveSystem> objectives;
    Mission(const std::string& map = ".......\n..V.M..\n.......", int stage = 4)
        : world(level(map), config.player), alarm(events, logger, world) {
        world.powerOn = stage >= 4;
        interaction.loadBank(world, config);
        objectives =
            std::make_unique<ObjectiveSystem>(events, world, interaction, alarm, config, stage);
    }
    void tick(float dt, bool held = false, bool modified = false) {
        interaction.update(dt, held, world, modified);
        objectives->update(dt);
        events.dispatch();
    }
    void cash() { world.player.pos = world.player.prevPos = world.level.map.tileCenter({4, 1}); }
    void open() { tick(config.mission.crack, true); }
};
}  // namespace

TEST(ObjectiveSystem, QuietRouteRequiresPowerAndOpensAllVaultTilesOnce) {
    Mission run(".......\n..VV.M.\n.......");
    int completed = 0;
    run.events.subscribe<ObjectiveCompleted>([&](const auto& event) {
        EXPECT_EQ(event.stageId, "S4");
        ++completed;
    });
    run.world.powerOn = false;
    run.tick(25, true);
    EXPECT_FALSE(run.objectives->vaultOpen());
    run.world.powerOn = true;
    run.tick(25, true);
    EXPECT_TRUE(run.objectives->vaultOpen());
    EXPECT_TRUE(run.world.level.map.isOpen(2, 1));
    EXPECT_TRUE(run.world.level.map.isOpen(3, 1));
    EXPECT_FALSE(run.world.alarmLoud);
    run.tick(1, true);
    EXPECT_EQ(completed, 1);
    EXPECT_EQ(run.objectives->stage(), 5);
}

TEST(ObjectiveSystem, QuietProgressDecaysAndNoiseResumesAccumulatedHoldTime) {
    Mission run;
    int pulses = 0;
    run.events.subscribe<NoiseEmitted>([&](const auto& event) {
        if (event.type == NoiseType::Crack) {
            ++pulses;
            EXPECT_EQ(event.radius, 200);
        }
    });
    run.tick(4, true);
    EXPECT_EQ(pulses, 0);
    run.tick(3);
    EXPECT_NEAR(run.interaction.progress(), 3.f / 25, 1e-6);
    run.tick(1, true);
    EXPECT_EQ(pulses, 1);
    run.world.player.pos = {24, 24};
    run.tick(3, true);
    EXPECT_EQ(pulses, 1);
    EXPECT_FALSE(run.objectives->vaultOpen());
}

TEST(ObjectiveSystem, ThermiteUsesModifiedControlRaisesAlarmAndBurnsFor75Seconds) {
    Mission run;
    int alarms = 0;
    run.events.subscribe<AlarmTriggered>([&](const auto& event) {
        EXPECT_EQ(event.reason, AlarmReason::Thermite);
        ++alarms;
    });
    run.world.powerOn = false;
    run.tick(2, true, true);
    EXPECT_FALSE(run.world.alarmLoud);
    EXPECT_FLOAT_EQ(run.objectives->thermiteRemaining(), 0);
    run.world.powerOn = true;
    run.tick(2, true, true);
    EXPECT_EQ(alarms, 1);
    EXPECT_TRUE(run.world.alarmLoud);
    EXPECT_FALSE(run.objectives->vaultOpen());
    // Placement does not consume any of the subsequent burn duration.
    EXPECT_FLOAT_EQ(run.objectives->thermiteRemaining(), 75);
    run.tick(74);
    EXPECT_FALSE(run.objectives->vaultOpen());
    run.tick(1);
    EXPECT_TRUE(run.objectives->vaultOpen());
    EXPECT_EQ(run.objectives->bags()[0].dye, DyeState::Armed);
    EXPECT_EQ(alarms, 1);
}

TEST(ObjectiveSystem, QuietCrackingRemainsAvailableWithPowerAfterAlarm) {
    Mission run;
    run.alarm.trigger(AlarmReason::Combat);
    run.events.dispatch();
    run.open();
    EXPECT_TRUE(run.objectives->vaultOpen());
    EXPECT_TRUE(run.world.alarmLoud);
}

TEST(ObjectiveSystem, DyeDisarmPreservesValueArmedPickupSpoilsAndSingleBagLimitHolds) {
    Mission run("........\n..V.MM..\n........");
    run.open();
    run.cash();
    run.tick(2, true, true);
    EXPECT_EQ(run.objectives->bags()[0].dye, DyeState::Disarmed);
    run.tick(0.01f, true);
    EXPECT_TRUE(run.world.player.carryingBag());
    EXPECT_FLOAT_EQ(run.objectives->bags()[0].value, 20000);
    EXPECT_EQ(run.objectives->stage(), 6);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({5, 1});
    run.tick(0.01f, true);
    EXPECT_EQ(run.objectives->bags()[1].state, BagState::Stack);
    run.objectives->throwBag({0, 1});
    run.tick(0.01f, true);
    EXPECT_EQ(run.objectives->bags()[1].state, BagState::Carried);
    EXPECT_FLOAT_EQ(run.objectives->bags()[1].value, 10000);
}

TEST(ObjectiveSystem, ArmedDyeBurstsAt45SecondsExactlyOnceAndDisarmedDyeSurvives) {
    Mission run("........\n..V.MM..\n........");
    int bursts = 0;
    run.events.subscribe<DyePackBurst>([&](const auto&) { ++bursts; });
    run.open();
    run.cash();
    run.tick(2, true, true);
    run.tick(42.99f);
    EXPECT_EQ(bursts, 0);
    run.tick(0.011f);
    EXPECT_EQ(bursts, 1);
    EXPECT_EQ(run.objectives->bags()[0].dye, DyeState::Disarmed);
    run.tick(100);
    EXPECT_EQ(bursts, 1);
    EXPECT_FLOAT_EQ(run.objectives->bags()[1].value, 10000);
}

TEST(ObjectiveSystem, ThrowClipsFirstObstacleAndDroppedBagRetainsIdentityAndValue) {
    Mission run("..........\n..V.M.#...\n..........");
    run.open();
    run.cash();
    run.tick(0.01f, true);
    int dropped = 0, picked = 0;
    run.events.subscribe<BagDropped>([&](const auto& event) {
        EXPECT_EQ(event.value, 10000);
        ++dropped;
    });
    run.events.subscribe<BagPicked>([&](const auto& event) {
        EXPECT_EQ(event.value, 10000);
        ++picked;
    });
    run.objectives->throwBag({1, 0});
    run.events.dispatch();
    const auto& bag = run.objectives->bags()[0];
    EXPECT_EQ(dropped, 1);
    EXPECT_LT(bag.pos.x, 6 * 48);
    EXPECT_GT(bag.pos.x, 5 * 48);
    EXPECT_EQ(bag.state, BagState::Dropped);
    run.world.player.pos = run.world.player.prevPos = bag.pos;
    run.tick(0.01f, true);
    EXPECT_EQ(picked, 1);
    EXPECT_EQ(bag.state, BagState::Carried);
    EXPECT_TRUE(run.world.player.carryingBag());
}

TEST(ObjectiveSystem, ClearThrowUses300PixelsAndInvalidOrEmptyThrowDoesNothing) {
    Mission run(std::string(20, '.') + "\n..V.M" + std::string(15, '.') + "\n" +
                std::string(20, '.'));
    run.open();
    run.cash();
    run.tick(0.01f, true);
    const auto before = run.world.player.pos;
    run.objectives->throwBag({0, 0});
    EXPECT_TRUE(run.world.player.carryingBag());
    run.objectives->throwBag({1, 0});
    EXPECT_FLOAT_EQ(run.objectives->bags()[0].pos.x, before.x + 300);
    run.objectives->throwBag({1, 0});
    EXPECT_FLOAT_EQ(run.objectives->bags()[0].pos.x, before.x + 300);
}

TEST(ObjectiveSystem, StageOrderRequiresServiceDoorCrossingAndDoesNotSkipEvents) {
    Mission run("........\n..S.kBV.\n........", 1);
    std::vector<std::string> stages;
    run.events.subscribe<ObjectiveCompleted>(
        [&](const auto& event) { stages.push_back(event.stageId); });
    run.world.level.map.setOpen(2, 1, true);
    run.tick(0.1f);
    EXPECT_EQ(run.objectives->stage(), 1);
    run.world.player.prevPos = {72, 72};
    run.world.player.pos = {168, 72};
    run.tick(0.1f);
    EXPECT_EQ(run.objectives->stage(), 2);
    run.world.player.collectKeycard();
    run.world.powerOn = true;
    run.tick(0.1f);
    EXPECT_EQ(run.objectives->stage(), 4);
    EXPECT_EQ(stages, (std::vector<std::string>{"S1", "S2", "S3"}));
}

TEST(ObjectiveSystem, EveryShippedStagePresetRestoresRequiredWorldStateAndArmedLoot) {
    std::ostringstream output;
    Logger logger(output, "");
    Config config;
    for (int stage = 1; stage <= 6; ++stage) {
        auto loaded = LevelLoader::load("assets/levels/gotham_central.json", logger);
        ASSERT_TRUE(loaded);
        World world(std::move(*loaded), config.player);
        ObjectiveSystem::applyPreset(world, config.mission, stage);
        EventBus bus;
        InteractionSystem interaction(bus);
        AlarmDirector alarm(bus, logger, world);
        interaction.loadBank(world, config);
        ObjectiveSystem objectives(bus, world, interaction, alarm, config, stage);
        EXPECT_EQ(objectives.stage(), stage);
        EXPECT_EQ(world.player.hasKeycard(), stage >= 3);
        EXPECT_EQ(world.powerOn, stage >= 4);
        EXPECT_EQ(objectives.vaultOpen(), stage >= 5);
        EXPECT_EQ(objectives.bags().size(), 10u);
        EXPECT_FALSE(world.player.carryingBag());
        EXPECT_EQ(objectives.pickedCount(), 0);
        for (const auto& bag : objectives.bags())
            EXPECT_EQ(bag.dye, stage >= 5 ? DyeState::Armed : DyeState::Unarmed);
        if (stage >= 3) {
            const auto point = config.mission.retryPositions[stage - 3];
            const auto pos = world.level.map.tileCenter({point.x, point.y});
            EXPECT_FLOAT_EQ(world.player.pos.x, pos.x);
            EXPECT_FLOAT_EQ(world.player.pos.y, pos.y);
        }
        int completion = 0;
        bus.subscribe<ObjectiveCompleted>([&](const auto&) { ++completion; });
        objectives.update(0.01f);
        bus.dispatch();
        EXPECT_EQ(completion, 0);
    }
}

TEST(ObjectiveSystem, CarryMultiplierAppliesToWalkSprintAndCrouch) {
    for (int mode = 0; mode < 3; ++mode) {
        auto map =
            level(std::string(40, '.') + "\n" + std::string(40, '.') + "\n" + std::string(40, '.'))
                .map;
        PlayerConfig config;
        Player player({72, 72}, config);
        player.setCarryingBag(true);
        Input input;
        input.move = {1, 0};
        input.sprintHeld = mode == 1;
        input.crouchPressed = mode == 2;
        player.update(1, input, map);
        EXPECT_FLOAT_EQ(player.velocity().x, (mode == 0   ? config.walk
                                              : mode == 1 ? config.sprint
                                                          : config.crouch) *
                                                 config.bagSpeedMult);
    }
}

TEST(ObjectiveSystem, AlarmOpenedFrontDoorStillBlocksPlayerUntilS6) {
    auto map = level(".....\n..F..\n.....").map;
    map.setOpen(2, 1, true);
    Player player({72, 72}, PlayerConfig{});
    Input input;
    input.move = {1, 0};
    player.update(0.5f, input, map);
    EXPECT_LE(player.pos.x, 2 * 48 - player.radius);
    EXPECT_GT(map.moveCircle({72, 72}, {80, 0}, 14).x, 2 * 48);
    player.unlockFrontExit();
    player.update(0.5f, input, map);
    EXPECT_GT(player.pos.x, 2 * 48);
}

TEST(ObjectiveSystem, ClosedDoorsStopThrowsUntilOpened) {
    Mission run("..........\n..V.M.d...\n......#...\n..........");
    run.open();
    run.cash();
    run.tick(0.01f, true);
    run.objectives->throwBag({1, 0});
    EXPECT_LT(run.objectives->bags()[0].pos.x, 6 * 48);
    run.world.player.pos = run.world.player.prevPos = run.objectives->bags()[0].pos;
    run.tick(0.01f, true);
    run.world.level.map.setOpen(6, 1, true);
    run.objectives->throwBag({1, 0});
    EXPECT_GT(run.objectives->bags()[0].pos.x, 6 * 48);
}

TEST(ObjectiveSystem, InteractionRangeIs48PixelsEvenOnLargerTiles) {
    auto loaded = level(".....\n.....\n.....");
    std::istringstream source(".....\n.....\n.....");
    loaded.map = TileMap::parse(source, 64);
    World world(std::move(loaded), PlayerConfig{});
    EventBus events;
    InteractionSystem interaction(events);
    int collected = 0;
    interaction.add({"pickup",
                     {world.player.pos.x + 60, world.player.pos.y},
                     0,
                     "E",
                     [](const Player&) { return true; },
                     [&](World&) { ++collected; }});
    interaction.update(0.01f, true, world);
    EXPECT_EQ(collected, 0);
    world.player.pos.x += 12;
    interaction.update(0.01f, true, world);
    EXPECT_EQ(collected, 1);
}

TEST(ObjectiveSystem, BollardsHoldAndVanDelayCannotBeSkippedOrRestarted) {
    Mission run("...........\n..N.b.Z.v..\n...........", 6);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({2, 1});
    run.tick(3, true);
    EXPECT_FALSE(run.objectives->bollardsLowered());
    run.tick(1, true);
    EXPECT_TRUE(run.objectives->bollardsLowered());
    EXPECT_TRUE(run.world.level.map.isOpen(4, 1));
    EXPECT_FALSE(run.objectives->vanArrived());
    run.tick(9.9f, true);
    EXPECT_FALSE(run.objectives->vanArrived());
    run.tick(.1f);
    EXPECT_TRUE(run.objectives->vanArrived());
    run.tick(100, true);
    EXPECT_TRUE(run.objectives->vanArrived());
}
TEST(ObjectiveSystem, CarriedAndThrownCashDeliverExactlyOnceButVanRequiredToLeave) {
    Mission run("............\n..N.b.Z.v.M.\n............", 6);
    int delivered = 0, completed = 0;
    run.events.subscribe<BagDelivered>([&](const auto& event) {
        ++delivered;
        EXPECT_EQ(event.value, 10000);
    });
    run.events.subscribe<MissionComplete>([&](const auto&) { ++completed; });
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({6, 1});
    run.tick(1, true);
    EXPECT_EQ(completed, 0);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({9, 1});
    run.tick(.01f, true);
    ASSERT_TRUE(run.world.player.carryingBag());
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({6, 1});
    run.tick(.01f);
    EXPECT_EQ(delivered, 1);
    EXPECT_FALSE(run.world.player.carryingBag());
    run.tick(1, true);
    EXPECT_EQ(completed, 0);
    EXPECT_EQ(delivered, 1);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({2, 1});
    run.tick(4, true);
    run.tick(10);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({5, 1});
    run.tick(.01f, true);
    EXPECT_FALSE(run.objectives->complete());
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({6, 1});
    run.tick(.01f, true);
    EXPECT_EQ(completed, 1);
    EXPECT_TRUE(run.objectives->complete());
    run.tick(1, true);
    EXPECT_EQ(completed, 1);
    EXPECT_EQ(delivered, 1);
}
TEST(ObjectiveSystem, BagThrownIntoPickupZoneDeliversWithoutVanAndCannotBeRecollected) {
    Mission run(".............\n..V.M.....Zv.\n.............", 5);
    run.cash();
    run.tick(.01f, true);
    run.objectives->throwBag({1, 0});
    run.tick(.01f);
    EXPECT_EQ(run.objectives->deliveredCount(), 1);
    EXPECT_EQ(run.objectives->bags()[0].state, BagState::Delivered);
    run.world.player.pos = run.world.player.prevPos = run.objectives->bags()[0].pos;
    run.tick(.01f, true);
    EXPECT_FALSE(run.world.player.carryingBag());
    EXPECT_FALSE(run.objectives->complete());
}
TEST(ObjectiveSystem, VanDoesNotAllowDepartureWithoutCashOrOutsidePickupZone) {
    Mission run("...........\n..N.b.Z.v..\n...........", 6);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({2, 1});
    run.tick(4, true);
    run.tick(10);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({6, 1});
    run.tick(1, true);
    EXPECT_FALSE(run.objectives->complete());
}

TEST(ObjectiveSystem, DepartureCreditsNewCarriedBagBeforeMissionCompleteOnSameTick) {
    Mission run(".............\n..N.M.M.Z.v..\n.............", 6);
    int credited = 0, atCompletion = 0;
    run.events.subscribe<BagDelivered>([&](const auto&) { ++credited; });
    run.events.subscribe<MissionComplete>([&](const auto&) { atCompletion = credited; });
    run.cash();
    run.tick(.01f, true);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({8, 1});
    run.tick(.01f);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({2, 1});
    run.tick(4, true);
    run.tick(10);
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({6, 1});
    run.tick(.01f, true);
    ASSERT_TRUE(run.world.player.carryingBag());
    run.world.player.pos = run.world.player.prevPos = run.world.level.map.tileCenter({8, 1});
    run.tick(.01f, true);
    EXPECT_EQ(atCompletion, 2);
    EXPECT_EQ(run.objectives->deliveredCount(), 2);
}
