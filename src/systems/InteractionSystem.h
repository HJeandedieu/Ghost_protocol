#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/Vec2.h"

class Player;
class EventBus;
class Config;
struct World;

struct Interactable {
    std::string id;
    Vec2 position;
    float holdSeconds = 0;
    std::string prompt;
    std::function<bool(const Player&)> canInteract;
    std::function<void(World&)> onComplete;
    bool decayOnLeave = false;
};

class InteractionSystem {
   public:
    explicit InteractionSystem(EventBus& bus);
    void add(Interactable item);
    void loadBank(World& world, const Config& config);
    void update(float dt, bool held, World& world);
    float progress() const;
    const Interactable* target() const;
    bool targetAvailable(const World& world) const;
    bool claimedThisTick() const { return claimedThisTick_; }

   private:
    EventBus& bus_;
    std::vector<Interactable> items_;
    std::vector<float> elapsed_;
    int target_ = -1;
    float lockpickNoise_ = 0;
    bool claimedThisTick_ = false;
    void openNormalDoors(World& world);
};
