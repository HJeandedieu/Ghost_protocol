#include "entities/GuardAI.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "world/Pathfinder.h"
#include "world/Raycast.h"

namespace {
constexpr float kPi = 3.14159265358979323846f;
float distance(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
}  // namespace

GuardAI::GuardAI(Guard& guard, TileMap& map, EventBus& events, Logger& logger)
    : guard_(guard), map_(map), logger_(logger), pathfinder_(guard.visionConfig(), map) {
    investigatePath_.reserve(static_cast<std::size_t>(map.width()) * map.height());
    events.subscribe<NoiseEmitted>([this](const NoiseEmitted& event) { hear(event); });
    events.subscribe<AlarmTriggered>([this](const auto&) {
        if (!guard_.dead() && guard_.state_ != GuardState::Unconscious) {
            guard_.state_ = GuardState::Combat;
            guard_.callInRemaining_ = 0;
            guard_.callInCompleted_ = true;
        }
    });
}

bool GuardAI::canReach(Vec2 target) const {
    pathfinder_.findPath(guard_.pos, target, map_, investigatePath_);
    return !investigatePath_.empty();
}

void GuardAI::hear(const NoiseEmitted& event) {
    if (!std::isfinite(event.radius) || event.radius <= 0 || event.sourceId == guard_.id ||
        distance(guard_.pos, event.origin) > event.radius || !std::isfinite(event.origin.x) ||
        !std::isfinite(event.origin.y))
        return;
    guard_.beginSuspicion(event.origin, true);
    looking_ = false;
    trackingProgress_ = false;
}

void GuardAI::recordCrumb() {
    const auto& config = guard_.config_;
    if (guard_.crumbs_.empty() || guard_.crumbs_.size() >= config.crumbMax) return;
    if (distance(guard_.pos, guard_.crumbs_.back()) >= config.crumbSpacing)
        guard_.crumbs_.push_back(guard_.pos);
}

bool GuardAI::moveToward(Vec2 target, float speed, float dt, bool record) {
    const float length = distance(guard_.pos, target);
    if (length <= guard_.config_.arriveTolerance) return true;
    if (speed <= 0) return false;
    const float step = std::min(length, speed * dt);
    const Vec2 delta{target.x - guard_.pos.x, target.y - guard_.pos.y};
    guard_.facing_ = std::atan2(delta.y, delta.x);
    guard_.move({delta.x / length * step, delta.y / length * step}, map_);
    if (record) recordCrumb();
    return distance(guard_.pos, target) <= guard_.config_.arriveTolerance;
}

bool GuardAI::stuck(Vec2 target, float dt) {
    const float length = distance(guard_.pos, target);
    if (!trackingProgress_ || target.x != stuckTarget_.x || target.y != stuckTarget_.y) {
        trackingProgress_ = true;
        stuckTarget_ = target;
        stuckElapsed_ = 0;
        progress_.clear();
        progress_.emplace_back(0, length);
    }
    stuckElapsed_ += dt;
    progress_.emplace_back(stuckElapsed_, length);
    if (stuckElapsed_ < guard_.config_.stuckWindow) return false;
    const double start = stuckElapsed_ - guard_.config_.stuckWindow;
    while (progress_.size() > 1 && progress_[1].first <= start) progress_.pop_front();
    float previousDistance = progress_.front().second;
    if (progress_.size() > 1 && progress_[1].first > progress_.front().first) {
        const float fraction = static_cast<float>((start - progress_.front().first) /
                                                  (progress_[1].first - progress_.front().first));
        previousDistance += (progress_[1].second - previousDistance) * fraction;
    }
    return previousDistance - length < guard_.config_.stuckMinProgress;
}

void GuardAI::startSearching(Vec2 center) {
    guard_.state_ = GuardState::Searching;
    searchCenter_ = center;
    searchElapsed_ = pauseRemaining_ = 0;
    trackingProgress_ = false;
    viaCenter_ = false;
    searchIndex_ = 0;
    searchPoints_.clear();
    const float radius = guard_.config_.searchLoopRadius;
    const std::array<Vec2, 4> candidates{{{center.x + radius, center.y},
                                          {center.x, center.y + radius},
                                          {center.x - radius, center.y},
                                          {center.x, center.y - radius}}};
    for (const auto point : candidates)
        if (Raycast::isPathClear(center, point, guard_.radius, map_, guard_.config_.pathClearStep))
            searchPoints_.push_back(point);
    if (!searchPoints_.empty()) {
        const auto nearest = std::min_element(
            searchPoints_.begin(), searchPoints_.end(),
            [&](Vec2 a, Vec2 b) { return distance(guard_.pos, a) < distance(guard_.pos, b); });
        std::rotate(searchPoints_.begin(), nearest, searchPoints_.end());
    }
}

void GuardAI::update(float deltaTime) {
    if (guard_.dead()) return;
    if (!std::isfinite(deltaTime) || deltaTime <= 0) return;
    double dt = deltaTime;
    auto& guard = guard_;
    const auto& config = guard.config_;
    guard.prevPos = guard.pos;
    while (dt > 0) {
        if (guard.state_ == GuardState::Patrol) {
            guard.update(dt, map_);
            return;
        }
        if (guard.state_ == GuardState::Suspicious) {
            // Detection owns the timer for suspicion caused only by vision.
            if (!guard.noiseInterest_) return;
            const double consumed = std::min<double>(
                dt, std::max(0.0f, config.suspiciousTime - guard.suspiciousElapsed_));
            guard.suspiciousElapsed_ += consumed;
            dt -= consumed;
            if (guard.suspiciousElapsed_ < config.suspiciousTime) return;
            if (!canReach(guard.interestPoint_)) {
                guard.returnToRoute();
            } else {
                guard.state_ = GuardState::Investigating;
                looking_ = false;
                trackingProgress_ = false;
                pathIndex_ = 0;
            }
            continue;
        }
        if (guard.state_ == GuardState::Investigating && !looking_ &&
            distance(guard.pos, guard.interestPoint_) <= config.arriveTolerance) {
            looking_ = true;
            lookElapsed_ = 0;
            arrivalHeading_ = guard.facing_;
            trackingProgress_ = false;
        }
        if (guard.state_ == GuardState::Investigating && looking_) {
            const double consumed =
                std::min(dt, std::max(0.0, config.investigateLook - lookElapsed_));
            lookElapsed_ += consumed;
            dt -= consumed;
            guard.facing_ = arrivalHeading_ +
                            config.lookSweepDeg * kPi / 180 *
                                (config.investigateLook > 0
                                     ? std::sin(2 * kPi * lookElapsed_ / config.investigateLook)
                                     : 0);
            if (lookElapsed_ >= config.investigateLook)
                startSearching(guard.interestPoint_);
            else
                return;
            continue;
        }
        if (guard.state_ == GuardState::Searching && searchElapsed_ >= config.searchTime) {
            guard.returnToRoute();
            trackingProgress_ = false;
            continue;
        }
        // Short movement steps retain breadcrumbs even during a long update.
        const float speed =
            guard.state_ == GuardState::Returning ? config.patrolSpeed : config.searchSpeed;
        double slice = std::min(dt, config.pathClearStep > 0 && speed > 0
                                        ? static_cast<double>(config.pathClearStep) / speed
                                        : dt);
        if (guard.state_ == GuardState::Searching)
            slice = std::min(slice, config.searchTime - searchElapsed_);
        if (guard.state_ == GuardState::Searching && pauseRemaining_ > 0)
            slice = std::min(slice, pauseRemaining_);
        if (guard.state_ == GuardState::Investigating) {
            const Vec2 target = pathIndex_ < investigatePath_.size() ? investigatePath_[pathIndex_]
                                                                     : guard.interestPoint_;
            // Start the progress window before movement, including its first step.
            if (!trackingProgress_) stuck(target, 0);
            if (moveToward(target, speed, slice, true)) {
                ++pathIndex_;
                trackingProgress_ = false;
                if (pathIndex_ >= investigatePath_.size()) {
                    looking_ = true;
                    lookElapsed_ = 0;
                    arrivalHeading_ = guard.facing_;
                }
            } else if (stuck(target, slice))
                startSearching(guard.pos);
        } else if (guard.state_ == GuardState::Searching) {
            searchElapsed_ += slice;
            if (pauseRemaining_ > 0) {
                pauseRemaining_ = std::max(0.0, pauseRemaining_ - slice);
            } else if (searchIndex_ >= searchPoints_.size()) {
                guard.facing_ += config.searchTurnRate * kPi / 180 * slice;
            } else {
                const auto point = searchPoints_[searchIndex_];
                if (!viaCenter_ && !Raycast::isPathClear(guard.pos, point, guard.radius, map_,
                                                         config.pathClearStep))
                    viaCenter_ = true;
                const auto target = viaCenter_ ? searchCenter_ : point;
                if (moveToward(target, speed, slice, true)) {
                    if (viaCenter_)
                        viaCenter_ = false;
                    else {
                        guard.facing_ =
                            std::atan2(point.y - searchCenter_.y, point.x - searchCenter_.x);
                        pauseRemaining_ = config.searchPointPause;
                        ++searchIndex_;
                    }
                }
            }
        } else if (guard.state_ == GuardState::Returning) {
            while (!guard.crumbs_.empty() &&
                   distance(guard.pos, guard.crumbs_.back()) <= config.arriveTolerance)
                guard.crumbs_.pop_back();
            const Vec2 target = guard.crumbs_.empty() ? guard.routePosition_ : guard.crumbs_.back();
            if (!trackingProgress_) stuck(target, 0);
            if (moveToward(target, speed, slice, false)) {
                trackingProgress_ = false;
                if (guard.crumbs_.empty()) guard.restorePatrol();
            } else if (stuck(target, slice)) {
                logger_.log(LogLevel::Warn, guard.id + ": stuck returning; snapping to breadcrumb");
                guard.pos = target;
                trackingProgress_ = false;
            }
        } else
            return;
        dt -= slice;
    }
    // A timeout on the exact tick boundary must enter Returning immediately.
    if (guard.state_ == GuardState::Searching && searchElapsed_ + 1e-9 >= config.searchTime) {
        guard.returnToRoute();
        trackingProgress_ = false;
    }
}
