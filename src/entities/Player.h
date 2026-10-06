#pragma once

#include <vector>

#include "core/Config.h"
#include "core/Input.h"
#include "entities/Entity.h"

class TileMap;
class Guard;
class EventBus;

class Player : public Entity {
   public:
    Player(Vec2 spawn, const PlayerConfig& config);
    void update(float dt, const Input& input, const TileMap& map);
    bool tryTakedown(std::vector<Guard>& guards, EventBus& events);
    Vec2 velocity() const { return velocity_; }
    bool isCrouched() const { return crouched_; }
    bool isSprinting() const { return sprinting_; }
    Vec2 interpolatedPosition(float alpha) const;
    bool hasKeycard() const { return hasKeycard_; }
    void collectKeycard() { hasKeycard_ = true; }

   private:
    const PlayerConfig config_;
    Vec2 velocity_;
    bool crouched_ = false;
    bool sprinting_ = false;
    bool hasKeycard_ = false;
};
