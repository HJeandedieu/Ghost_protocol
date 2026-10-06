#pragma once

#include <vector>

#include "core/Config.h"

class EventBus;
class Player;
class Laser;

class LaserSystem {
   public:
    LaserSystem(EventBus& events, const LaserConfig& config, float noiseRadius);
    void update(float dt, const Player& player, const std::vector<Laser>& lasers, bool disabled);
    int count() const { return count_; }

   private:
    EventBus& events_;
    const LaserConfig config_;
    const float noiseRadius_;
    double cooldown_ = 0;
    double window_ = 0;
    int count_ = 0;
    void reset();
};
