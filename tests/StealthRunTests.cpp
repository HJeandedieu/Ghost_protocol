#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <queue>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "core/Rng.h"
#include "entities/GuardAI.h"
#include "systems/AlarmDirector.h"
#include "systems/DetectionSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/LaserSystem.h"
#include "systems/NoiseSystem.h"
#include "systems/ObjectiveSystem.h"
#include "systems/PagerSystem.h"
#include "systems/ScoreSystem.h"
#include "systems/VisionSystem.h"
#include "world/LevelLoader.h"
#include "world/Pathfinder.h"
#include "world/World.h"

namespace {
// Exercises the same fixed-tick ordering as PlayState, without rendering or teleporting.
class StealthRun {
   public:
    std::ostringstream console;
    Logger logger{console, ""};
    Config config = Config::load("assets/config/tuning.json", logger);
    World world{load(), config.player, config.guard, config.camera};
    EventBus events;
    NoiseSystem noise{events};
    InteractionSystem interaction{events};
    DetectionSystem detection{events, logger, world.guards, config.difficulty.normal.detectFill};
    AlarmDirector alarm{events, logger, world};
    PagerSystem pagers{events, config.pager, world, interaction};
    LaserSystem lasers{events, config.laser, config.noise.laser};
    std::vector<std::unique_ptr<GuardAI>> ai;
    std::unique_ptr<ObjectiveSystem> objectives;
    ScoreSystem score{config.payout};
    MissionRun stats;
    int completions = 0;
    bool passMiddleLaser = false;
    bool allowLoud = false;
    bool clearReturn = false;
    float elapsed = 0;
    int laserTouches = 0;
    StealthRun() {
        interaction.loadBank(world, config);
        objectives = std::make_unique<ObjectiveSystem>(events, world, interaction, alarm, config);
        events.subscribe<BagDelivered>([this](const auto& event) { score.addBag(event.value); });
        events.subscribe<AlarmTriggered>([this](const auto&) { stats.alarmEver = true; });
        events.subscribe<MissionComplete>([this](const auto&) { ++completions; });
        detection.bindCameras(world.cameras);
        for (auto& guard : world.guards)
            ai.push_back(std::make_unique<GuardAI>(guard, world.level.map, events, logger));
        events.subscribe<LaserTouched>([this](const auto&) { ++laserTouches; });
        Input input;
        input.crouchPressed = true;
        tick(input);
    }
    Level load() {
        auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
        if (!level) throw std::runtime_error("Shipped level failed to load");
        return std::move(*level);
    }
    void tick(const Input& input = {}) {
        constexpr float dt = 1.0f / 60;
        noise.beginTick();
        world.player.update(dt, input, world.level.map);
        if (world.player.pos.x != world.player.prevPos.x ||
            world.player.pos.y != world.player.prevPos.y)
            noise.emit(world.player.pos, config.noise.crouch, NoiseType::Step, world.player.id);
        pagers.update(dt);
        interaction.update(dt, input.interactHeld, world, input.sprintHeld);
        for (auto& guardAi : ai) guardAi->update(dt);
        detection.update(dt, world.player, world.level.map, world.guards);
        for (auto& camera : world.cameras) camera.update(dt);
        const bool disabled = world.securityLoopRemaining > 0;
        detection.updateCameras(dt, world.player, world.level.map, world.cameras, config.guard,
                                disabled);
        lasers.update(dt, world.player, world.lasers, disabled);
        VisionSystem(config.guard).findBodies(world.guards, world.level.map, events);
        alarm.update();
        objectives->update(dt);
        stats.advance(dt, !objectives->complete());
        events.dispatch();
        elapsed += dt;
    }
    TileCoord locate(TileType type) const {
        for (int y = 0; y < world.level.map.height(); ++y)
            for (int x = 0; x < world.level.map.width(); ++x)
                if (world.level.map.tile(x, y) == type) return {x, y};
        return {-1, -1};
    }
    bool safe(Vec2 point) const {
        if (allowLoud && world.alarmLoud) return true;
        auto player = world.player;
        player.pos = point;
        VisionSystem vision(config.guard);
        for (const auto& guard : world.guards)
            if (guard.id != "G02" && guard.id != "G03" &&
                !(clearReturn && (guard.id == "G05" || guard.id == "G06" || guard.id == "G07" ||
                                  guard.id == "G08")) &&
                vision.sees(guard, player, world.level.map))
                return false;
        if (world.securityLoopRemaining > 0) return true;
        for (const auto& camera : world.cameras)
            if (vision.sees(camera.pos, camera.facing(), camera.coneDegrees() / 2, camera.range(),
                            point, world.level.map))
                return false;
        for (const auto& laser : world.lasers) {
            const auto near = laser.nearestPoint(point);
            if (passMiddleLaser && laser.id == "L02") continue;
            if (std::hypot(near.x - point.x, near.y - point.y) <= player.radius + 2) return false;
        }
        return true;
    }
    bool walk(TileCoord goal) {
        const auto& map = world.level.map;
        const int width = map.width();
        const auto index = [width](TileCoord p) { return p.y * width + p.x; };
        Vec2 waypoint = world.player.pos;
        bool traveling = false;
        for (int attempt = 0; attempt < 18000 && (!world.alarmLoud || allowLoud); ++attempt) {
            // Use normal takedowns to clear threats; answer ringing pagers on the return route.
            const auto staff =
                std::find_if(world.guards.begin(), world.guards.end(), [this](const auto& guard) {
                    return (!world.alarmLoud &&
                            (guard.id == "G02" || guard.id == "G03" ||
                             (clearReturn && (guard.id == "G05" || guard.id == "G06" ||
                                              guard.id == "G07" || guard.id == "G08")))) &&
                           guard.state() != GuardState::Unconscious &&
                           (guard.detection() > 0 || (clearReturn && guard.id == "G05")) &&
                           std::hypot(guard.pos.x - world.player.pos.x,
                                      guard.pos.y - world.player.pos.y) <
                               (clearReturn ? config.guard.rangeLit : 160);
                });
            if (staff != world.guards.end() && staff->state() != GuardState::Unconscious &&
                (staff->detection() > 0 || (clearReturn && staff->id == "G05")) &&
                std::hypot(staff->pos.x - world.player.pos.x, staff->pos.y - world.player.pos.y) <
                    (clearReturn ? config.guard.rangeLit : 160)) {
                if (world.player.tryTakedown(world.guards, events)) {
                    tick();
                    traveling = false;
                } else {
                    Input input;
                    input.sprintHeld = clearReturn;
                    input.crouchPressed = clearReturn && world.player.isCrouched();
                    input.move = {staff->pos.x - world.player.pos.x,
                                  staff->pos.y - world.player.pos.y};
                    tick(input);
                }
                continue;
            }
            if (clearReturn) {
                const auto pager = std::find_if(
                    world.guards.begin(), world.guards.end(),
                    [this](const auto& g) { return pagers.state(g.id) == PagerState::Ringing; });
                if (pager != world.guards.end()) {
                    Input answer;
                    const float distance = std::hypot(pager->pos.x - world.player.pos.x,
                                                      pager->pos.y - world.player.pos.y);
                    if (distance > 32) {
                        answer.move = {pager->pos.x - world.player.pos.x,
                                       pager->pos.y - world.player.pos.y};
                        answer.sprintHeld = true;
                        answer.crouchPressed = world.player.isCrouched();
                    } else
                        answer.interactHeld = true;
                    tick(answer);
                    traveling = false;
                    continue;
                }
                if (!world.player.isCrouched()) {
                    Input crouch;
                    crouch.crouchPressed = true;
                    tick(crouch);
                    traveling = false;
                    continue;
                }
            }
            const Vec2 target = map.tileCenter(goal);
            if (std::hypot(target.x - world.player.pos.x, target.y - world.player.pos.y) < 2) {
                for (int i = 0; i < 12; ++i) tick();
                return !world.alarmLoud || allowLoud;
            }
            if (traveling &&
                std::hypot(waypoint.x - world.player.pos.x, waypoint.y - world.player.pos.y) > 2) {
                Input input;
                input.move = {waypoint.x - world.player.pos.x, waypoint.y - world.player.pos.y};
                tick(input);
                continue;
            }
            traveling = false;
            const TileCoord start{static_cast<int>(world.player.pos.x / map.tileSize()),
                                  static_cast<int>(world.player.pos.y / map.tileSize())};
            std::vector<int> previous(width * map.height(), -1);
            std::queue<TileCoord> frontier;
            frontier.push(start);
            previous[index(start)] = index(start);
            while (!frontier.empty() && previous[index(goal)] < 0) {
                const auto p = frontier.front();
                frontier.pop();
                for (const auto offset :
                     {TileCoord{1, 0}, TileCoord{-1, 0}, TileCoord{0, 1}, TileCoord{0, -1}}) {
                    const TileCoord next{p.x + offset.x, p.y + offset.y};
                    if (!map.contains(next.x, next.y) || !map.isPassable(next.x, next.y) ||
                        previous[index(next)] >= 0 || !safe(map.tileCenter(next)))
                        continue;
                    previous[index(next)] = index(p);
                    frontier.push(next);
                }
            }
            Input input;
            if (previous[index(goal)] >= 0) {
                int next = index(goal);
                while (previous[next] != index(start) && next != index(start))
                    next = previous[next];
                waypoint = map.tileCenter({next % width, next / width});
                traveling = true;
                input.move = {waypoint.x - world.player.pos.x, waypoint.y - world.player.pos.y};
            }
            tick(input);
        }
        return false;
    }
    void hold(float seconds, bool modified = false) {
        Input input;
        input.interactHeld = true;
        input.sprintHeld = modified;
        for (int i = 0; i < static_cast<int>(std::ceil(seconds * 60)); ++i) tick(input);
    }
};
}  // namespace

class StealthRoute : public testing::TestWithParam<int> {};

void reachGate(StealthRun& run, int delay) {
    // A patient route observes the initial patrol before leaving the alley.
    for (int tick = 0; tick < (delay + 1) * 120; ++tick) run.tick();
    auto service = run.locate(TileType::ServiceDoor);
    ASSERT_TRUE(run.walk({service.x - 1, service.y})) << run.elapsed << "\n" << run.console.str();
    Input approach;
    approach.move = {1, 0};
    for (int i = 0; i < 6; ++i) run.tick(approach);
    run.hold(run.config.mission.lockpick + 0.1f);
    ASSERT_TRUE(run.world.level.map.isOpen(service.x, service.y));
    ASSERT_TRUE(run.walk(run.locate(TileType::Keycard))) << run.elapsed << "\n"
                                                         << run.console.str();
    run.hold(0.1f);
    ASSERT_TRUE(run.world.player.hasKeycard());
    const auto panel = run.locate(TileType::SecurityPanel);
    ASSERT_TRUE(run.walk(panel)) << run.elapsed << "\n" << run.console.str();
    run.hold(run.config.mission.securityHold + 0.1f);
    ASSERT_TRUE(run.world.securityLoopUsed);
    const auto red = run.locate(TileType::CardDoor);
    ASSERT_TRUE(run.walk({red.x - 1, red.y})) << run.elapsed << "\n" << run.console.str();
    for (int i = 0; i < 6; ++i) run.tick(approach);
    run.hold(0.1f);
    ASSERT_TRUE(run.world.level.map.isOpen(red.x, red.y));
    ASSERT_TRUE(run.walk(run.locate(TileType::Breaker))) << run.elapsed << "\n"
                                                         << run.console.str();
    run.hold(run.config.mission.breakerHold + 0.1f);
    ASSERT_TRUE(run.world.powerOn);
    const auto gate = run.locate(TileType::Gate);
    ASSERT_TRUE(run.walk({gate.x + 1, gate.y - 1})) << run.elapsed << "\n" << run.console.str();
    EXPECT_FALSE(run.world.alarmLoud);
    EXPECT_EQ(run.laserTouches, 0);
}

TEST_P(StealthRoute, ShippedBankSilentRouteWithActiveGuardsAndHazards) {
    StealthRun run;
    reachGate(run, GetParam());
    EXPECT_TRUE(run.world.powerOn);
}

INSTANTIATE_TEST_SUITE_P(TenPatrolStartTimes, StealthRoute, testing::Range(0, 10));

TEST(StealthMission, FullSilentHeistDeliversAndPaysOutWithActiveGuardVision) {
    StealthRun run;
    reachGate(run, 0);
    ASSERT_TRUE(run.world.powerOn);
    const auto vault = run.locate(TileType::VaultDoor);
    ASSERT_TRUE(run.walk({vault.x + 1, vault.y + 1})) << run.console.str();
    run.hold(run.config.mission.crack + .1f);
    ASSERT_TRUE(run.objectives->vaultOpen());
    const auto cash = run.locate(TileType::Money);
    ASSERT_TRUE(run.walk(cash)) << run.console.str();
    run.hold(run.config.mission.dyeHold + .1f, true);
    run.hold(.1f);
    ASSERT_TRUE(run.world.player.carryingBag());
    ASSERT_EQ(run.objectives->stage(), 6);
    run.passMiddleLaser = true;
    ASSERT_TRUE(run.walk({58, 10}));
    ASSERT_TRUE(run.walk({58, 12}));
    ASSERT_TRUE(run.walk({58, 14}));
    ASSERT_TRUE(run.walk({52, 15})) << run.console.str();
    const auto patrol = std::find_if(run.world.guards.begin(), run.world.guards.end(),
                                     [](const auto& g) { return g.id == "G05"; });
    for (int i = 0; i < 5400 && (std::abs(patrol->pos.x / 48 - 52.5f) > 1 ||
                                 std::abs(patrol->pos.y / 48 - 18.5f) > 1);
         ++i)
        run.tick();
    run.clearReturn = true;
    ASSERT_TRUE(run.walk({52, 22})) << run.console.str();
    const auto foyer = std::find_if(run.world.guards.begin(), run.world.guards.end(),
                                    [](const auto& guard) { return guard.id == "G08"; });
    ASSERT_NE(foyer, run.world.guards.end());
    ASSERT_TRUE(
        run.walk({static_cast<int>(foyer->pos.x / 48) - 1, static_cast<int>(foyer->pos.y / 48)}))
        << "pos " << run.world.player.pos.x << "," << run.world.player.pos.y << " elapsed "
        << run.elapsed << " alarm " << run.world.alarmLoud << " loop "
        << run.world.securityLoopRemaining << run.console.str();
    Input behind;
    behind.move = {1, 0};
    for (int i = 0; i < 16; ++i) run.tick(behind);
    if (foyer->state() != GuardState::Unconscious) {
        ASSERT_TRUE(run.world.player.tryTakedown(run.world.guards, run.events));
    }
    run.tick();
    ASSERT_TRUE(run.walk(run.locate(TileType::BollardPanel))) << run.elapsed << run.console.str();
    run.hold(run.config.mission.bollardHold + .1f);
    ASSERT_TRUE(run.walk(run.locate(TileType::PickupZone))) << run.console.str();
    for (int i = 0; i < 600; ++i) run.tick();
    run.hold(.1f);
    EXPECT_EQ(run.completions, 1);
    EXPECT_EQ(run.objectives->deliveredCount(), 1);
    EXPECT_FALSE(run.stats.alarmEver);
    Rng rng(42);
    const auto payout = run.score.finalize(!run.stats.alarmEver, run.stats.seconds, 0, rng);
    EXPECT_GT(payout.subtotal, 0);
    EXPECT_GT(payout.ghostBonus, 0);
    EXPECT_GT(payout.finalAmount, 0);
}

class MissionPhaseRoute : public testing::TestWithParam<bool> {};
TEST_P(MissionPhaseRoute, FullShippedObjectiveRouteCompletesAfterThermiteOrLateAlarm) {
    StealthRun run;
    reachGate(run, 0);
    ASSERT_TRUE(run.world.powerOn);
    const auto vault = run.locate(TileType::VaultDoor);
    ASSERT_TRUE(run.walk({vault.x + 1, vault.y + 1}));
    run.allowLoud = true;
    if (GetParam()) {
        run.hold(run.config.mission.thermitePlace + .1f, true);
        ASSERT_TRUE(run.world.alarmLoud);
        for (int i = 0; i < static_cast<int>(std::ceil(run.config.mission.thermiteBurn * 60)); ++i)
            run.tick();
    } else
        run.hold(run.config.mission.crack + .1f);
    ASSERT_TRUE(run.objectives->vaultOpen());
    ASSERT_TRUE(run.walk(run.locate(TileType::Money)));
    run.hold(.1f);
    ASSERT_TRUE(run.world.player.carryingBag());
    if (!GetParam()) {
        run.alarm.trigger(AlarmReason::Shot);
        run.tick();
    }
    ASSERT_TRUE(run.world.alarmLoud);
    ASSERT_TRUE(run.walk(run.locate(TileType::BollardPanel)));
    run.hold(run.config.mission.bollardHold + .1f);
    ASSERT_TRUE(run.walk(run.locate(TileType::PickupZone)));
    for (int i = 0; i < 600; ++i) run.tick();
    run.hold(.1f);
    EXPECT_EQ(run.completions, 1);
    EXPECT_TRUE(run.stats.alarmEver);
    Rng rng(42);
    const auto payout = run.score.finalize(!run.stats.alarmEver, run.stats.seconds, 0, rng);
    EXPECT_EQ(payout.ghostBonus, 0);
    EXPECT_GT(payout.finalAmount, 0);
}
INSTANTIATE_TEST_SUITE_P(ThermiteAndMixed, MissionPhaseRoute, testing::Bool());
