#pragma once

#include <deque>
#include <utility>
#include <vector>

#include "core/Vec2.h"

class Guard;
class TileMap;
class EventBus;
class Logger;
struct NoiseEmitted;

class GuardAI {
   public:
    GuardAI(Guard& guard, TileMap& map, EventBus& events, Logger& logger);
    GuardAI(const GuardAI&) = delete;
    GuardAI& operator=(const GuardAI&) = delete;
    bool canReach(Vec2 target) const;
    void update(float dt);
    const std::vector<Vec2>& searchPoints() const { return searchPoints_; }

   private:
    Guard& guard_;
    TileMap& map_;
    Logger& logger_;
    std::vector<Vec2> searchPoints_;
    Vec2 searchCenter_{};
    std::size_t searchIndex_ = 0;
    bool viaCenter_ = false;
    bool looking_ = false;
    double lookElapsed_ = 0;
    float arrivalHeading_ = 0;
    double searchElapsed_ = 0;
    double pauseRemaining_ = 0;
    double stuckElapsed_ = 0;
    std::deque<std::pair<double, float>> progress_;
    Vec2 stuckTarget_{};
    bool trackingProgress_ = false;
    void hear(const NoiseEmitted& event);
    void startSearching(Vec2 center);
    bool moveToward(Vec2 target, float speed, float dt, bool record);
    bool stuck(Vec2 target, float dt);
    void recordCrumb();
};
