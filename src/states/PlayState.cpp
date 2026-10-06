#include "states/PlayState.h"

#include <cmath>
#include <utility>

#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "systems/VisionSystem.h"

PlayState::PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed,
                     Renderer& renderer, Logger& logger)
    : world_(std::move(level), config.player, config.guard),
      input_(input),
      camera_(world_.player.pos, config.view),
      seed_(seed),
      ripple_(config.ping, world_.level.map),
      renderer_(renderer),
      noise_(events_),
      interaction_(events_),
      detection_(events_, logger, world_.guards, config.difficulty.normal.detectFill),
      config_(config) {
    interaction_.loadBank(world_, config_);
    renderer_.prepareLevel(world_.level);
    std::vector<Hearer> hearers;
    hearers.reserve(world_.level.guards.size());
    revealables_.reserve(world_.guards.size());
    for (auto& guard : world_.guards) {
        guardAi_.push_back(std::make_unique<GuardAI>(guard, world_.level.map, events_, logger));
        hearers.push_back({guard.id, guard.pos});
        revealables_.push_back(&guard);
    }
    noise_.setHearers(std::move(hearers));
}

void PlayState::enter() {}
void PlayState::exit() {}
void PlayState::update(float dt) {
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
    interaction_.update(dt, input_.interactHeld || input_.interactPressed, world_);
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
    VisionSystem(config_.guard).findBodies(world_.guards, world_.level.map, events_);
    ripple_.update(dt, world_.level.map, revealables_);
    events_.dispatch();
#ifndef NDEBUG
    if (input_.debugPressed) debugView_ = !debugView_;
#endif
}
void PlayState::render(float alpha) {
    renderer_.drawLevel(world_.level, world_.player, ripple_, camera_.interpolatedTarget(alpha),
                        facing_, alpha, debugView_, seed_, world_.guards);
    renderer_.drawInteractionHud(world_, interaction_, noise_.currentRadius(),
                                 config_.noise.sprint);
}
