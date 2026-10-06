#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <queue>
#include <sstream>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/GuardAI.h"
#include "systems/AlarmDirector.h"
#include "systems/DetectionSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/LaserSystem.h"
#include "systems/NoiseSystem.h"
#include "systems/PagerSystem.h"
#include "systems/VisionSystem.h"
#include "world/LevelLoader.h"
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
    float elapsed = 0;
    int laserTouches = 0;
    StealthRun() {
        interaction.loadBank(world, config);
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
        interaction.update(dt, input.interactHeld, world);
        for (auto& guardAi : ai) guardAi->update(dt);
        detection.update(dt, world.player, world.level.map, world.guards);
        for (auto& camera : world.cameras) camera.update(dt);
        const bool disabled = world.securityLoopRemaining > 0;
        detection.updateCameras(dt, world.player, world.level.map, world.cameras, config.guard,
                                disabled);
        lasers.update(dt, world.player, world.lasers, disabled);
        VisionSystem(config.guard).findBodies(world.guards, world.level.map, events);
        alarm.update();
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
        auto player = world.player;
        player.pos = point;
        VisionSystem vision(config.guard);
        for (const auto& guard : world.guards)
            if (guard.id != "G02" && guard.id != "G03" &&
                vision.sees(guard, player, world.level.map))
                return false;
        if (world.securityLoopRemaining > 0) return true;
        for (const auto& camera : world.cameras)
            if (vision.sees(camera.pos, camera.facing(), camera.coneDegrees() / 2, camera.range(),
                            point, world.level.map))
                return false;
        for (const auto& laser : world.lasers) {
            const auto near = laser.nearestPoint(point);
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
        for (int attempt = 0; attempt < 18000 && !world.alarmLoud; ++attempt) {
            // These two guards have no pagers; normal takedowns clear the staff/security route.
            const auto staff =
                std::find_if(world.guards.begin(), world.guards.end(), [this](const auto& guard) {
                    return (guard.id == "G02" || guard.id == "G03") &&
                           guard.state() != GuardState::Unconscious && guard.detection() > 0 &&
                           std::hypot(guard.pos.x - world.player.pos.x,
                                      guard.pos.y - world.player.pos.y) < 160;
                });
            if (staff != world.guards.end() && staff->state() != GuardState::Unconscious &&
                staff->detection() > 0 &&
                std::hypot(staff->pos.x - world.player.pos.x, staff->pos.y - world.player.pos.y) <
                    160) {
                if (world.player.tryTakedown(world.guards, events)) {
                    tick();
                    traveling = false;
                } else {
                    Input input;
                    input.move = {staff->pos.x - world.player.pos.x,
                                  staff->pos.y - world.player.pos.y};
                    tick(input);
                }
                continue;
            }
            const Vec2 target = map.tileCenter(goal);
            if (std::hypot(target.x - world.player.pos.x, target.y - world.player.pos.y) < 2) {
                for (int i = 0; i < 12; ++i) tick();
                return !world.alarmLoud;
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
    void hold(float seconds) {
        Input input;
        input.interactHeld = true;
        for (int i = 0; i < static_cast<int>(std::ceil(seconds * 60)); ++i) tick(input);
    }
};
}  // namespace

class StealthRoute : public testing::TestWithParam<int> {};

TEST_P(StealthRoute, ShippedBankSilentRouteWithActiveGuardsAndHazards) {
    StealthRun run;
    // A patient route observes the initial patrol before leaving the alley.
    for (int tick = 0; tick < (GetParam() + 1) * 120; ++tick) run.tick();
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

INSTANTIATE_TEST_SUITE_P(TenPatrolStartTimes, StealthRoute, testing::Range(0, 10));
