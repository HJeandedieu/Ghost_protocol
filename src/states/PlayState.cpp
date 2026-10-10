#include "states/PlayState.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "systems/AudioDirector.h"
#include "systems/VisionSystem.h"
#include "systems/VoiceDirector.h"
#include "world/Raycast.h"

namespace {
Config missionConfig(Config config, const Level& level, const std::vector<EnemySpec>& enemies,
                     Logger& logger) {
    config.view = validateViewForLevel(
        config.view, static_cast<float>(level.map.tileSize()),
        std::hypot(static_cast<float>(level.map.width() * level.map.tileSize()),
                   static_cast<float>(level.map.height() * level.map.tileSize())),
        logger);
    const auto shield = std::find_if(enemies.begin(), enemies.end(),
                                     [](const auto& spec) { return spec.id == "shield_cop"; });
    validateShotGeometry(config.shotGeometry, config.view,
                         shield == enemies.end() ? 0 : shield->radius, logger);
    return config;
}
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
                     std::function<void(int, bool)> busted, AudioDirector* audio,
                     VoiceDirector* voice)
    : config_(missionConfig(config, level, enemies, logger)),
      world_(missionWorld(std::move(level), config_, stage)),
      input_(input),
      view_(config_.view),
      seed_(seed),
      ripple_(config.ping, world_.level.map),
      renderer_(renderer),
      noise_(events_),
      interaction_(events_),
      detection_(events_, logger, world_.guards, difficulty.detectFill),
      alarm_(events_, logger, world_),
      pagers_(events_, config.pager, world_, interaction_),
      lasers_(events_, config.laser, config.noise.laser),
      combat_(events_, weapons, seed, loadout, config_.shotGeometry, config_.view.wallHeight),
      pickups_(events_, config.pickup, seed),
      enemyCombat_(events_, world_, combat_, pickups_, enemies, config_, seed, difficulty.enemyDmg),
      waves_(events_, world_, enemies, waves, entries,
             [&] {
                 auto alarm = config.alarm;
                 alarm.maxAlive = difficulty.maxAlive;
                 return alarm;
             }()),
      alarmSequence_(events_, config.alarm, seed),
      retry_(std::move(retry)),
      run_(run ? std::move(run) : std::make_shared<MissionRun>()),
      finish_(std::move(finish)),
      score_(config.payout),
      payoutRng_(seed),
      pause_(std::move(pause)),
      busted_(std::move(busted)),
      audio_(audio),
      voice_(voice) {
    if (voice_) voice_->listen(events_);
    if (audio_) audio_->listen(events_);
    noise_.setWeapons(weapons, config.noise);
    noise_.setEnemies(enemies);
    interaction_.loadBank(world_, config_);
    if (audio_ || voice_) {
        const auto& map = world_.level.map;
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x) {
                const auto type = map.tile(x, y);
                const auto id = std::to_string(x) + ":" + std::to_string(y);
                if (type == TileType::ServiceDoor || type == TileType::CardDoor)
                    if (audio_) audio_->bindInteraction(id, "door_open");
                if (type == TileType::Keycard) {
                    if (audio_) audio_->bindInteraction(id, "keycard_pick");
                    if (voice_) voice_->bindInteraction(id, "V09");
                }
                if (type == TileType::VaultDoor && voice_)
                    voice_->bindInteraction("vault:" + id + ":thermite", "V15");
                if (type == TileType::Breaker) {
                    if (audio_) audio_->bindInteraction(id, "gate_open");
                    if (voice_) voice_->bindInteraction(id, "V11");
                }
            }
    }
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

void PlayState::enter() {
    if (voice_) {
        voice_->setTutorialContext(objectives_->stage(), renderer_.hints(), world_.player.id);
        voice_->request("V02");
    }
}
void PlayState::exit() {}
void PlayState::pauseForCaptureLoss() {
    inputGate_.blockFire();
    if (!world_.player.dead() && pause_) pause_(objectives_->stage(), world_.alarmLoud);
}
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
    if (audio_) audio_->clearLoops();
    const bool vaultBefore = objectives_->vaultOpen(),
               bollardsBefore = objectives_->bollardsLowered(),
               vanBefore = objectives_->vanArrived();
    const float cooldownBefore = ripple_.cooldownRemaining();
    const bool reloadBefore = combat_.activeWeapon().reloadRemaining() > 0;
    noise_.beginTick();
    auto& player = world_.player;
    view_.look(input_.mouseDelta);
    facing_ = view_.yawDeg() * (3.14159265358979323846f / 180);
    Input movementInput = input_;
    movementInput.move = view_.movement(input_.move);
    player.update(dt, movementInput, world_.level.map);
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
    if (input_.throwPressed) objectives_->throwBag({std::cos(facing_), std::sin(facing_)});
    const float before = ripple_.cooldownRemaining();
    if (!world_.alarmLoud)
        ripple_.updateCharge(dt, input_.pingHeld, input_.pingPressed, player.pos);
    if (before <= 0 && ripple_.waveActive() && ripple_.waveRadius() == 0) {
        if (audio_)
            audio_->request(ripple_.maxRadius() > config_.ping.smallRadius ? "ping_big"
                                                                           : "ping_small");
        noise_.emit(ripple_.origin(), ripple_.maxRadius() * config_.ping.noiseMult, NoiseType::Ping,
                    player.id);
    }
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
    combat_.update(dt, inputGate_.filter(input_),
                   view_.shotRay(world_.player.pos, world_.player.isCrouched()), world_);
#ifndef NDEBUG
    if (input_.debugDamagePressed)
        combat_.applyDamage(world_.player, config_.player.armor, "debug");
#endif
    enemyCombat_.update(dt);
    renderer_.updateHealthHud(dt, world_.player);
    alarm_.update();
    Vec2 viewCenter = player.pos;
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
    SpawnView spawnView{viewCenter.x - halfWidth, viewCenter.y - halfHeight,
                        viewCenter.x + halfWidth, viewCenter.y + halfHeight};
    if (!debugView_) {
        spawnView.perspective = true;
        spawnView.footprint = ViewFootprint::perspective(
            view_.shotRay(player.pos, player.isCrouched()), view_.config().fovYDeg,
            static_cast<float>(Letterbox::kWidth) / Letterbox::kHeight, view_.config().nearClip,
            view_.config().farClip);
    }
    waves_.update(realDt, spawnView);
    objectives_->update(dt);
    if (audio_) {
        if (cooldownBefore > 0 && ripple_.cooldownRemaining() <= 0) audio_->request("ping_ready");
        if (!reloadBefore && combat_.activeWeapon().reloadRemaining() > 0)
            audio_->request("reload");
        if (!vaultBefore && objectives_->vaultOpen()) audio_->request("vault_open");
        if (!bollardsBefore && objectives_->bollardsLowered()) audio_->request("bollard_lower");
        if (!vanBefore && objectives_->vanArrived()) audio_->request("van_arrive");
        if (player.pos.x != player.prevPos.x || player.pos.y != player.prevPos.y)
            audio_->loop(player.isCrouched()    ? "step_crouch"
                         : player.isSprinting() ? "step_sprint"
                                                : "step_walk");
        if (objectives_->thermiteRemaining() > 0) audio_->loop("thermite_loop");
        const auto* target = interaction_.target();
        if (target && interaction_.claimedThisTick() && interaction_.progress() > 0) {
            if (target->id.rfind("vault:", 0) == 0 &&
                target->id.find(":thermite") == std::string::npos)
                audio_->loop("drill_pulse");
            const auto tile = world_.level.map.tile(
                static_cast<int>(target->position.x / world_.level.map.tileSize()),
                static_cast<int>(target->position.y / world_.level.map.tileSize()));
            if (tile == TileType::ServiceDoor) audio_->loop("lockpick_loop");
        }
    }
    events_.dispatch();
    if (voice_) {
        voice_->setTutorialContext(objectives_->stage(), renderer_.hints(), player.id);
        voice_->observeHealth(player.hp(), player.maximumHp());
        bool seenGuard = false;
        for (const auto& guard : world_.guards) {
            const TileCoord tile{static_cast<int>(guard.pos.x / world_.level.map.tileSize()),
                                 static_cast<int>(guard.pos.y / world_.level.map.tileSize())};
            if (!guard.dead() && guard.state() != GuardState::Unconscious &&
                (world_.alarmLoud || guard.reveal > 0 ||
                 ripple_.visibility(tile.x, tile.y, player.pos, world_.level.map) > 0) &&
                Raycast::hasLineOfSight(player.pos, guard.pos, world_.level.map))
                seenGuard = true;
        }
        const auto* target = interaction_.target();
        const bool service =
            target && world_.level.map.tile(
                          static_cast<int>(target->position.x / world_.level.map.tileSize()),
                          static_cast<int>(target->position.y / world_.level.map.tileSize())) ==
                          TileType::ServiceDoor;
        voice_->observeTutorial(seenGuard, service);
        for (const auto& laser : world_.lasers) {
            const auto point = laser.nearestPoint(player.pos);
            if (std::hypot(point.x - player.pos.x, point.y - player.pos.y) <=
                    config_.ping.hazardRevealRadius &&
                Raycast::hasLineOfSight(player.pos, point, world_.level.map))
                voice_->request("V12");
        }
        if (!vaultBefore && objectives_->vaultOpen()) {
            voice_->request(world_.alarmLoud ? "V16" : "V17");
            voice_->request("V18");
        }
        if (!bollardsBefore && objectives_->bollardsLowered()) voice_->request("V20");
        if (!vanBefore && objectives_->vanArrived()) voice_->request("V21");
    }
#ifndef NDEBUG
    if (input_.debugPressed) debugView_ = !debugView_;
#endif
}
void PlayState::render(float alpha) {
    if (debugView_)
        renderer_.drawLevel(
            world_.level, world_.player, ripple_, world_.player.interpolatedPosition(alpha),
            facing_, alpha, debugView_, seed_, world_.guards, world_.cameras, world_.lasers,
            world_.securityLoopRemaining > 0, &combat_, world_.pickups, world_.alarmLoud,
            world_.enemies, &enemyCombat_, &alarmSequence_, objectives_.get());
    else
        renderer_.drawPerspective(world_, view_, ripple_, alpha, &alarmSequence_, objectives_.get(),
                                  &combat_, &enemyCombat_);
    renderer_.drawInteractionHud(world_, interaction_, noise_.currentRadius(), config_.noise.sprint,
                                 objectives_.get());
    renderer_.drawWeaponHud(combat_);
    renderer_.drawPickupHud(pickups_.target(world_, interaction_.claimedThisTick()));
    renderer_.drawStealthHud(alarm_, pagers_, world_.guards);
    if (world_.alarmLoud) renderer_.drawWaveHud(waves_);
    renderer_.drawAlarmSequence(alarmSequence_);
    if (downed_ && !busted_) renderer_.drawBusted(objectives_->stage());
}
