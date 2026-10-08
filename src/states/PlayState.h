#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "core/EventBus.h"
#include "core/GameplayInputGate.h"
#include "core/Input.h"
#include "entities/GuardAI.h"
#include "entities/Player.h"
#include "render/AlarmSequence.h"
#include "render/FollowCamera.h"
#include "states/IState.h"
#include "systems/AlarmDirector.h"
#include "systems/CombatSystem.h"
#include "systems/DetectionSystem.h"
#include "systems/EnemyCombatSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/LaserSystem.h"
#include "systems/NoiseSystem.h"
#include "systems/ObjectiveSystem.h"
#include "systems/PagerSystem.h"
#include "systems/PickupSystem.h"
#include "systems/RippleSystem.h"
#include "systems/ScoreSystem.h"
#include "systems/WaveSpawner.h"
#include "world/Level.h"
#include "world/World.h"

class Renderer;
class Logger;
class AudioDirector;

class PlayState : public IState {
   public:
    PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed,
              Renderer& renderer, Logger& logger, const std::vector<WeaponSpec>& weapons,
              const std::vector<EnemySpec>& enemies, const std::vector<WaveSpec>& waves,
              const std::map<std::string, TileCoord>& entries, int stage = 1, bool loud = false,
              std::function<void(int, bool)> retry = {}, std::shared_ptr<MissionRun> run = {},
              std::function<void(Payout)> finish = {},
              std::array<std::string, 2> loadout = {{"whisper", "chatter"}},
              DifficultyPreset difficulty = {}, std::function<void(int, bool)> pause = {},
              std::function<void(int, bool)> busted = {}, AudioDirector* audio = nullptr);
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;
    const World& world() const { return world_; }
    const ObjectiveSystem& objectives() const { return *objectives_; }

   private:
    EventBus events_;
    World world_;
    std::vector<std::unique_ptr<GuardAI>> guardAi_;
    const Input& input_;
    FollowCamera camera_;
    float facing_ = 0.0f;
    std::uint32_t seed_;
    bool debugView_ = false;
    RippleSystem ripple_;
    std::vector<Entity*> revealables_;
    std::vector<Entity*> hazards_;
    Renderer& renderer_;
    NoiseSystem noise_;
    InteractionSystem interaction_;
    DetectionSystem detection_;
    AlarmDirector alarm_;
    PagerSystem pagers_;
    LaserSystem lasers_;
    CombatSystem combat_;
    PickupSystem pickups_;
    EnemyCombatSystem enemyCombat_;
    WaveSpawner waves_;
    AlarmSequence alarmSequence_;
    const Config& config_;
    std::unique_ptr<ObjectiveSystem> objectives_;
    std::function<void(int, bool)> retry_;
    bool downed_ = false;
    std::shared_ptr<MissionRun> run_;
    std::function<void(Payout)> finish_;
    ScoreSystem score_;
    Rng payoutRng_;
    std::function<void(int, bool)> pause_, busted_;
    bool bustedShown_ = false;
    GameplayInputGate inputGate_;
    AudioDirector* audio_ = nullptr;
};
