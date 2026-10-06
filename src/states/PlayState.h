#pragma once

#include <cstdint>
#include <memory>

#include "core/EventBus.h"
#include "core/Input.h"
#include "entities/GuardAI.h"
#include "entities/Player.h"
#include "render/FollowCamera.h"
#include "states/IState.h"
#include "systems/AlarmDirector.h"
#include "systems/CombatSystem.h"
#include "systems/DetectionSystem.h"
#include "systems/EnemyCombatSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/LaserSystem.h"
#include "systems/NoiseSystem.h"
#include "systems/PagerSystem.h"
#include "systems/PickupSystem.h"
#include "systems/RippleSystem.h"
#include "world/Level.h"
#include "world/World.h"

class Renderer;
class Logger;

class PlayState : public IState {
   public:
    PlayState(Level level, const Input& input, const Config& config, std::uint32_t seed,
              Renderer& renderer, Logger& logger, const std::vector<WeaponSpec>& weapons,
              const std::vector<EnemySpec>& enemies);
    void enter() override;
    void exit() override;
    void update(float dt) override;
    void render(float alpha) override;

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
    const Config& config_;
};
