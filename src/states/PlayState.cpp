#include "states/PlayState.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "systems/VisionSystem.h"

namespace {
World missionWorld(Level level, const Config& config, int stage) {
    World world(std::move(level), config.player, config.guard, config.camera);
    ObjectiveSystem::applyPreset(world, config.mission, stage);
    return world;
}
}  // namespace

PlayState::PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed,
                     Renderer& renderer, Logger& logger, const std::vector<WeaponSpec>& weapons,
                     const std::vector<EnemySpec>& enemies, const std::vector<WaveSpec>& waves,
                     const std::map<std::string, TileCoord>& entries, int stage, bool loud,
                     std::function<void(int, bool)> retry, std::shared_ptr<MissionRun> run,
                     std::function<void(Payout)> finish, std::array<std::string, 2> loadout,
                     DifficultyPreset difficulty, std::function<void(int, bool)> pause,
                     std::function<void(int, bool)> busted)
    : world_(missionWorld(std::move(level), config, stage)),
      input_(input),
      camera_(world_.player.pos, config.view),
      seed_(seed),
      ripple_(config.ping, world_.level.map),
      renderer_(renderer),
      noise_(events_),
      interaction_(events_),
      detection_(events_, logger, world_.guards, difficulty.detectFill),
      alarm_(events_, logger, world_),
      pagers_(events_, config.pager, world_, interaction_),
      lasers_(events_, config.laser, config.noise.laser),
      combat_(events_, weapons, seed, loadout),
      pickups_(events_, config.pickup, seed),
      enemyCombat_(events_, world_, combat_, pickups_, enemies, config, seed, difficulty.enemyDmg),
      waves_(events_, world_, enemies, waves, entries,
             [&] {
                 auto alarm = config.alarm;
                 alarm.maxAlive = difficulty.maxAlive;
                 return alarm;
             }()),
      alarmSequence_(events_, config.alarm, seed),
      config_(config),
      retry_(std::move(retry)),
      run_(run ? std::move(run) : std::make_shared<MissionRun>()),
      finish_(std::move(finish)),
      score_(config.payout),
      payoutRng_(seed),
      pause_(std::move(pause)),
      busted_(std::move(busted)) {
    noise_.setWeapons(weapons, config.noise);
    noise_.setEnemies(enemies);
    interaction_.loadBank(world_, config_);
    objectives_ =
        std::make_unique<ObjectiveSystem>(events_, world_, interaction_, alarm_, config_, stage);
    events_.subscribe<PlayerDowned>([this](const PlayerDowned&) {
        if (!downed_) ++run_->deaths;
        downed_ = true;
    });
    events_.subscribe<AlarmTriggered>([this](const auto&) { run_->alarmEver = true; });
    events_.subscribe<BagDelivered>([this](const auto& event) { score_.addBag(event.value); });
    events_.subscribe<MissionComplete>([this](const auto&) {
        if (finish_)
            finish_(score_.finalize(!run_->alarmEver, run_->seconds, run_->deaths, payoutRng_));
    });
    detection_.bindCameras(world_.cameras);
    renderer_.prepareLevel(world_.level);
    renderer_.resetHealthHud(world_.player);
    events_.subscribe<EntityDamaged>([this](const EntityDamaged& event) {
        if (event.targetId == world_.player.id) renderer_.notifyHealthDamage();
    });
    std::vector<Hearer> hearers;
    hearers.reserve(world_.level.guards.size());
    revealables_.reserve(world_.guards.size() + world_.cameras.size() + world_.lasers.size());
    hazards_.reserve(world_.cameras.size() + world_.lasers.size());
    for (auto& guard : world_.guards) {
        guardAi_.push_back(std::make_unique<GuardAI>(guard, world_.level.map, events_, logger));
        hearers.push_back({guard.id, guard.pos});
        revealables_.push_back(&guard);
    }
    noise_.setHearers(std::move(hearers));
    for (auto& camera : world_.cameras) hazards_.push_back(&camera);
    for (auto& laser : world_.lasers) hazards_.push_back(&laser);
    for (auto* hazard : hazards_) revealables_.push_back(hazard);
    if (loud) {
        // All guard listeners must exist before restoring the Loud state.
        alarm_.trigger(AlarmReason::Combat);
        events_.dispatch();
        alarmSequence_.restoreLoud();
    }
}

void PlayState::enter() {}
void PlayState::exit() {}
void PlayState::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    if (objectives_->complete()) return;
    if (!world_.player.dead() && input_.backPressed && pause_) {
        inputGate_.blockFire();
        pause_(objectives_->stage(), world_.alarmLoud);
        return;
    }
    if (downed_ && busted_ && !bustedShown_) {
        bustedShown_ = true;
        busted_(objectives_->stage(), world_.alarmLoud);
        return;
    }
    run_->advance(dt, !world_.player.dead());
    const float realDt = dt;
    dt = alarmSequence_.advance(realDt);
    if (world_.player.dead()) {
        if (downed_ && input_.confirmPressed && retry_)
            retry_(objectives_->stage(), world_.alarmLoud);
        renderer_.updateHealthHud(dt, world_.player);
        return;
    }
    noise_.beginTick();
    auto& player = world_.player;
    player.update(dt, input_, world_.level.map);
    if (input_.takedownPressed) player.tryTakedown(world_.guards, events_);
    if (player.pos.x != player.prevPos.x || player.pos.y != player.prevPos.y)
        noise_.emit(player.pos,
                    player.isCrouched()
                        ? config_.noise.crouch
                        : (player.isSprinting() ? config_.noise.sprint : config_.noise.walk),
                    NoiseType::Step, player.id);
    pagers_.update(dt);
    interaction_.update(dt, input_.interactHeld || input_.interactPressed, world_,
                        input_.sprintHeld);
    if (objectives_->complete()) {
        events_.dispatch();
        return;
    }
#ifndef NDEBUG
    if (input_.debugCopPressed) enemyCombat_.spawnDebugCop();
    if (input_.debugMedkitPressed)
        pickups_.spawn(PickupType::Medkit, {player.pos.x + player.radius * 2, player.pos.y},
                       world_);
    if (input_.debugArmorPressed)
        pickups_.spawn(PickupType::ArmorPlate, {player.pos.x, player.pos.y + player.radius * 2},
                       world_);
#endif
    pickups_.update(input_.interactPressed, interaction_.claimedThisTick(), world_, combat_);
    for (std::size_t i = 0; i < world_.guards.size(); ++i) {
        guardAi_[i]->update(dt);
        noise_.setHearerPosition(i, world_.guards[i].pos);
    }
    Vec2 cursorOffset{};
    if (input_.mouseInViewport && !debugView_) {
        cursorOffset = {
            camera_.target().x + input_.mouseLogical.x - Letterbox::kWidth * 0.5f - player.pos.x,
            camera_.target().y + input_.mouseLogical.y - Letterbox::kHeight * 0.5f - player.pos.y};
        if (cursorOffset.x != 0 || cursorOffset.y != 0)
            facing_ = std::atan2(cursorOffset.y, cursorOffset.x);
    }
    camera_.update(dt, player.pos, cursorOffset);
    if (input_.throwPressed) objectives_->throwBag({std::cos(facing_), std::sin(facing_)});
    const float before = ripple_.cooldownRemaining();
    if (!world_.alarmLoud)
        ripple_.updateCharge(dt, input_.pingHeld, input_.pingPressed, player.pos);
    if (before <= 0 && ripple_.waveActive() && ripple_.waveRadius() == 0)
        noise_.emit(ripple_.origin(), ripple_.maxRadius() * config_.ping.noiseMult, NoiseType::Ping,
                    player.id);
    detection_.update(dt, player, world_.level.map, world_.guards);
    for (auto& camera : world_.cameras) camera.update(dt);
    const bool looped = world_.securityLoopRemaining > 0;
    detection_.updateCameras(dt, player, world_.level.map, world_.cameras, config_.guard, looped);
    lasers_.update(dt, player, world_.lasers, looped);
    VisionSystem(config_.guard).findBodies(world_.guards, world_.level.map, events_);
    auto tickRevealables = revealables_;
    const auto lootRevealables = objectives_->revealables();
    tickRevealables.insert(tickRevealables.end(), lootRevealables.begin(), lootRevealables.end());
    for (auto& pickup : world_.pickups) tickRevealables.push_back(pickup.get());
    for (auto& enemy : world_.enemies) tickRevealables.push_back(enemy.get());
    ripple_.update(dt, world_.level.map, tickRevealables);
    if (!world_.alarmLoud) ripple_.applyProximity(player.pos, world_.level.map, hazards_);
    combat_.update(dt, inputGate_.filter(input_), facing_ * (180.0f / 3.14159265358979323846f),
                   world_);
#ifndef NDEBUG
    if (input_.debugDamagePressed)
        combat_.applyDamage(world_.player, config_.player.armor, "debug");
#endif
    enemyCombat_.update(dt);
    renderer_.updateHealthHud(dt, world_.player);
    alarm_.update();
    Vec2 viewCenter = camera_.target();
    float zoom = 1.f;
    if (debugView_) {
        const auto& map = world_.level.map;
        const float width = static_cast<float>(map.width() * map.tileSize());
        const float height = static_cast<float>(map.height() * map.tileSize());
        viewCenter = {width * 0.5f, height * 0.5f};
        zoom = std::min((Letterbox::kWidth - 48.f) / width, (Letterbox::kHeight - 128.f) / height);
    }
    const float halfWidth = Letterbox::kWidth * 0.5f / zoom;
    const float halfHeight = Letterbox::kHeight * 0.5f / zoom;
    waves_.update(realDt, {viewCenter.x - halfWidth, viewCenter.y - halfHeight,
                           viewCenter.x + halfWidth, viewCenter.y + halfHeight});
    objectives_->update(dt);
    events_.dispatch();
#ifndef NDEBUG
    if (input_.debugPressed) debugView_ = !debugView_;
#endif
}
void PlayState::render(float alpha) {
    renderer_.drawLevel(world_.level, world_.player, ripple_, camera_.interpolatedTarget(alpha),
                        facing_, alpha, debugView_, seed_, world_.guards, world_.cameras,
                        world_.lasers, world_.securityLoopRemaining > 0, &combat_, world_.pickups,
                        world_.alarmLoud, world_.enemies, &enemyCombat_, &alarmSequence_,
                        objectives_.get());
    renderer_.drawInteractionHud(world_, interaction_, noise_.currentRadius(), config_.noise.sprint,
                                 objectives_.get());
    renderer_.drawWeaponHud(combat_);
    renderer_.drawPickupHud(pickups_.target(world_, interaction_.claimedThisTick()));
    renderer_.drawStealthHud(alarm_, pagers_, world_.guards);
    if (world_.alarmLoud) renderer_.drawWaveHud(waves_);
    renderer_.drawAlarmSequence(alarmSequence_);
    if (downed_ && !busted_) renderer_.drawBusted(objectives_->stage());
}
