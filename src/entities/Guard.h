#pragma once

#include <vector>

#include "core/Config.h"
#include "entities/Entity.h"
#include "world/Level.h"

class EventBus;

enum class GuardState {
    Patrol,
    Suspicious,
    Investigating,
    Searching,
    Returning,
    Alerted,
    Combat,
    Unconscious
};

class Guard : public Entity {
   public:
    Guard(const GuardSpawn& spawn, const TileMap& map, const GuardConfig& config);
    void update(float dt, TileMap& map);
    float facing() const { return facing_; }
    Vec2 interpolatedPosition(float alpha) const;
    bool hasPager() const { return pager_; }
    PatrolMode mode() const { return mode_; }
    const GuardConfig& visionConfig() const { return config_; }
    GuardState state() const { return state_; }
    float detection() const { return detection_; }
    float callInRemaining() const { return callInRemaining_; }
    void beginSuspicion(Vec2 point, bool fromNoise);
    void returnToRoute();
    bool takeDown(EventBus& events);

   private:
    friend class DetectionSystem;
    friend class GuardAI;
    Vec2 interestPoint_{};
    bool noiseInterest_ = false;
    Vec2 routePosition_{};
    float routeHeading_ = 0;
    std::size_t routeWaypoint_ = 0;
    std::vector<Vec2> crumbs_;
    GuardState state_ = GuardState::Patrol;
    float detection_ = 0;
    float suspiciousElapsed_ = 0;
    float callInRemaining_ = 0;
    bool callInCompleted_ = false;
    const GuardConfig config_;
    std::vector<Vec2> waypoints_;
    PatrolMode mode_;
    bool pager_;
    std::size_t target_ = 0;
    int direction_ = 1;
    float facing_ = 0;
    void advanceWaypoint();
    void restorePatrol();
    Vec2 move(Vec2 displacement, TileMap& map);
};
