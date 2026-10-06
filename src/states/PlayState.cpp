#include "states/PlayState.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "systems/VisionSystem.h"

PlayState::PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed,
                     Renderer& renderer, Logger& logger, const std::vector<WeaponSpec>& weapons,
                     const std::vector<EnemySpec>& enemies, const std::vector<WaveSpec>& waves,
                     const std::map<std::string, TileCoord>& entries)
    : world_(std::move(level), config.player, config.guard, config.camera),
      input_(input),
      camera_(world_.player.pos, config.view),
      seed_(seed),
      ripple_(config.ping, world_.level.map),
      renderer_(renderer),
      noise_(events_),
      interaction_(events_),
      detection_(events_, logger, world_.guards, config.difficulty.normal.detectFill),
      alarm_(events_, logger, world_),
      pagers_(events_, config.pager, world_, interaction_),
      lasers_(events_, config.laser, config.noise.laser),
      combat_(events_, weapons, seed),
      pickups_(events_, config.pickup, seed),
      enemyCombat_(events_, world_, combat_, pickups_, enemies, config, seed),
      waves_(events_, world_, enemies, waves, entries, config.alarm),
      config_(config) {
    noise_.setWeapons(weapons, config.noise);
    noise_.setEnemies(enemies);
    interaction_.loadBank(world_, config_);
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
}

void PlayState::enter() {}
void PlayState::exit() {}
void PlayState::update(float dt) {
    if (world_.player.dead()) {
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
    interaction_.update(dt, input_.interactHeld || input_.interactPressed, world_);
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
    const float before = ripple_.cooldownRemaining();
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
    for (auto& pickup : world_.pickups) tickRevealables.push_back(pickup.get());
    for (auto& enemy : world_.enemies) tickRevealables.push_back(enemy.get());
    ripple_.update(dt, world_.level.map, tickRevealables);
    if (!world_.alarmLoud) ripple_.applyProximity(player.pos, world_.level.map, hazards_);
    combat_.update(dt, input_, facing_ * (180.0f / 3.14159265358979323846f), world_);
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
    waves_.update(dt, {viewCenter.x - halfWidth, viewCenter.y - halfHeight,
                       viewCenter.x + halfWidth, viewCenter.y + halfHeight});
    events_.dispatch();
#ifndef NDEBUG
    if (input_.debugPressed) debugView_ = !debugView_;
#endif
}
void PlayState::render(float alpha) {
    renderer_.drawLevel(world_.level, world_.player, ripple_, camera_.interpolatedTarget(alpha),
                        facing_, alpha, debugView_, seed_, world_.guards, world_.cameras,
                        world_.lasers, world_.securityLoopRemaining > 0, &combat_, world_.pickups,
                        world_.alarmLoud, world_.enemies, &enemyCombat_);
    renderer_.drawInteractionHud(world_, interaction_, noise_.currentRadius(),
                                 config_.noise.sprint);
    renderer_.drawWeaponHud(combat_);
    renderer_.drawPickupHud(pickups_.target(world_, interaction_.claimedThisTick()));
    renderer_.drawStealthHud(alarm_, pagers_, world_.guards);
    if (world_.alarmLoud) renderer_.drawWaveHud(waves_);
}
