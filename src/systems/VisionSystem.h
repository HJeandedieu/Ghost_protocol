#pragma once

#include "core/Config.h"
#include "core/Vec2.h"
#include "world/TileMap.h"

class Guard;
class Player;

class VisionSystem {
   public:
    explicit VisionSystem(const GuardConfig& config = {}) : config_(config) {}
    bool sees(const Vec2& eye, float facingDeg, float halfAngleDeg, float range, const Vec2& target,
              const TileMap& map) const;
    float rangeFor(LightLevel light, bool crouched) const;
    bool sees(const Guard& guard, const Player& player, const TileMap& map) const;

   private:
    const GuardConfig config_;
};
