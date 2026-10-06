#pragma once

#include <vector>

#include "core/Config.h"
#include "entities/Entity.h"
#include "world/Level.h"

class Guard : public Entity {
   public:
    Guard(const GuardSpawn& spawn, const TileMap& map, const GuardConfig& config);
    void update(float dt, TileMap& map);
    float facing() const { return facing_; }
    Vec2 interpolatedPosition(float alpha) const;
    bool hasPager() const { return pager_; }
    PatrolMode mode() const { return mode_; }
    const GuardConfig& visionConfig() const { return config_; }

   private:
    const GuardConfig config_;
    std::vector<Vec2> waypoints_;
    PatrolMode mode_;
    bool pager_;
    std::size_t target_ = 0;
    int direction_ = 1;
    float facing_ = 0;
    void advanceWaypoint();
};
