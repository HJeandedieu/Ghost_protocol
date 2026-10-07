#pragma once

#include <vector>

#include "core/Config.h"
#include "entities/Bag.h"

class EventBus;
class AlarmDirector;
class InteractionSystem;
struct World;

class ObjectiveSystem {
   public:
    ObjectiveSystem(EventBus& events, World& world, InteractionSystem& interaction,
                    AlarmDirector& alarm, const Config& config, int stage = 1);
    static void applyPreset(World& world, const MissionConfig& config, int stage);
    void update(float dt);
    void throwBag(Vec2 direction);
    int stage() const { return stage_; }
    const char* objective() const;
    bool vaultOpen() const { return vaultOpen_; }
    float thermiteRemaining() const { return thermiteRemaining_; }
    float thermiteAge() const { return thermiteAge_; }
    Vec2 vaultPosition() const { return vaultPosition_; }
    int pickedCount() const;
    int deliveredCount() const;
    bool bollardsLowered() const { return bollardsLowered_; }
    bool vanArrived() const {
        return bollardsLowered_ && vanAge_ + 1e-6 >= config_.mission.vanDelay;
    }
    float vanRemaining() const;
    bool complete() const { return complete_; }
    const std::vector<Bag>& bags() const { return bags_; }
    std::vector<Entity*> revealables();

   private:
    void openVault();
    void spoil(Bag& bag);
    void collect(std::size_t index);
    void completeStage();
    EventBus& events_;
    World& world_;
    InteractionSystem& interaction_;
    AlarmDirector& alarm_;
    const Config& config_;
    int stage_ = 1;
    bool vaultOpen_ = false;
    bool openedThisTick_ = false;
    bool thermitePlacedThisTick_ = false;
    bool pickedAny_ = false;
    bool bollardsLowered_ = false;
    bool loweredThisTick_ = false;
    bool complete_ = false;
    double vanAge_ = 0;
    bool serviceCrossed_ = false;
    bool approachedService_ = false;
    Vec2 vaultPosition_{};
    float thermiteRemaining_ = 0;
    double thermiteAge_ = 0;
    double dyeAge_ = 0;
    double crackNoiseAge_ = 0;
    std::vector<Bag> bags_;
};
