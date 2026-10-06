#pragma once

#include <vector>

#include "core/Config.h"
#include "core/Vec2.h"

class TileMap;
class Entity;

class RippleSystem {
   public:
    RippleSystem(const PingConfig& config, const TileMap& map);
    void startPing(Vec2 origin, float chargeSeconds);
    void updateCharge(float dt, bool held, bool pressed, Vec2 origin);
    void update(float dt, const TileMap& map, std::vector<Entity*>& entities);
    void applyProximity(Vec2 playerPos, const TileMap& map, std::vector<Entity*>& hazards);
    float tileReveal(int x, int y) const;
    float visibility(int x, int y, Vec2 player, const TileMap& map) const;
    float cooldownRemaining() const { return cooldown_; }
    float cooldownFraction() const;
    bool charging() const { return charging_; }
    float chargeSeconds() const { return charge_; }
    bool waveActive() const { return active_; }
    float waveRadius() const { return radius_; }
    float maxRadius() const { return maximum_; }
    Vec2 origin() const { return origin_; }
    float haloRadius() const { return config_.halo; }

   private:
    const PingConfig config_;
    int width_;
    int height_;
    std::vector<float> reveal_;
    Vec2 origin_;
    float radius_ = 0;
    float maximum_ = 0;
    float cooldown_ = 0;
    float charge_ = 0;
    bool active_ = false;
    bool charging_ = false;
};
