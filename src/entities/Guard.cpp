#include "entities/Guard.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

Guard::Guard(const GuardSpawn& spawn, const TileMap& map, const GuardConfig& config)
    : config_(config), mode_(spawn.mode), pager_(spawn.pager) {
    if (spawn.waypoints.empty()) throw std::invalid_argument("Guard requires a waypoint");
    id = spawn.id;
    radius = config.radius;
    waypoints_.reserve(spawn.waypoints.size());
    for (const auto point : spawn.waypoints) waypoints_.push_back(map.tileCenter(point));
    pos = prevPos = waypoints_.front();
    facing_ = spawn.facing * kPi / 180;
    if (mode_ != PatrolMode::Stationary && waypoints_.size() > 1) {
        target_ = 1;
        facing_ = std::atan2(waypoints_[1].y - pos.y, waypoints_[1].x - pos.x);
    }
}

void Guard::advanceWaypoint() {
    if (mode_ == PatrolMode::Loop) {
        target_ = (target_ + 1) % waypoints_.size();
    } else {
        if (target_ == waypoints_.size() - 1) direction_ = -1;
        if (target_ == 0) direction_ = 1;
        if (direction_ > 0)
            ++target_;
        else
            --target_;
    }
}

void Guard::beginSuspicion(Vec2 point, bool fromNoise) {
    if (state_ == GuardState::Alerted || state_ == GuardState::Combat ||
        state_ == GuardState::Unconscious || !std::isfinite(point.x) || !std::isfinite(point.y))
        return;
    if (state_ == GuardState::Patrol) {
        routePosition_ = pos;
        routeHeading_ = facing_;
        routeWaypoint_ = target_;
        crumbs_.clear();
        crumbs_.push_back(pos);
    }
    state_ = GuardState::Suspicious;
    suspiciousElapsed_ = 0;
    interestPoint_ = point;
    noiseInterest_ = fromNoise;
    if (point.x != pos.x || point.y != pos.y)
        facing_ = std::atan2(point.y - pos.y, point.x - pos.x);
}

void Guard::restorePatrol() {
    pos = routePosition_;
    facing_ = routeHeading_;
    target_ = routeWaypoint_;
    crumbs_.clear();
    noiseInterest_ = false;
    state_ = GuardState::Patrol;
}

void Guard::returnToRoute() {
    noiseInterest_ = false;
    state_ = GuardState::Returning;
    if (crumbs_.size() <= 1 &&
        std::hypot(pos.x - routePosition_.x, pos.y - routePosition_.y) <= config_.arriveTolerance)
        restorePatrol();
}

Vec2 Guard::move(Vec2 displacement, TileMap& map) {
    pos = map.moveCircle(pos, displacement, radius);
    const float size = static_cast<float>(map.tileSize());
    const int tx = static_cast<int>(std::floor(pos.x / size));
    const int ty = static_cast<int>(std::floor(pos.y / size));
    for (int y = ty - 1; y <= ty + 1; ++y)
        for (int x = tx - 1; x <= tx + 1; ++x) {
            if (map.tile(x, y) != TileType::Door || map.isOpen(x, y)) continue;
            const float dx = pos.x - std::clamp(pos.x, x * size, (x + 1) * size);
            const float dy = pos.y - std::clamp(pos.y, y * size, (y + 1) * size);
            if (dx * dx + dy * dy <= radius * radius) map.setOpen(x, y, true);
        }
    return pos;
}

void Guard::update(float dt, TileMap& map) {
    if (!std::isfinite(dt) || dt <= 0) return;
    prevPos = pos;
    if (state_ != GuardState::Patrol) return;
    if (mode_ == PatrolMode::Stationary) {
        facing_ = std::fmod(facing_ + config_.stationaryTurnSpeed * kPi / 180 * dt, 2 * kPi);
        return;
    }
    if (waypoints_.size() < 2 || config_.patrolSpeed <= 0) return;
    float remaining = config_.patrolSpeed * dt;
    std::size_t duplicates = 0;
    while (remaining > 0) {
        const auto destination = waypoints_[target_];
        const Vec2 delta{destination.x - pos.x, destination.y - pos.y};
        const float distance = std::hypot(delta.x, delta.y);
        if (distance == 0) {
            advanceWaypoint();
            if (++duplicates >= waypoints_.size()) break;
            continue;
        }
        duplicates = 0;
        facing_ = std::atan2(delta.y, delta.x);
        const float step = std::min(remaining, distance);
        const Vec2 displacement{delta.x / distance * step, delta.y / distance * step};
        const Vec2 requested{pos.x + displacement.x, pos.y + displacement.y};
        const auto next = move(displacement, map);
        if (next.x != requested.x || next.y != requested.y) break;
        remaining -= step;
        if (step == distance) {
            pos = destination;
            advanceWaypoint();
        }
    }
}

Vec2 Guard::interpolatedPosition(float alpha) const {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    return {prevPos.x + (pos.x - prevPos.x) * alpha, prevPos.y + (pos.y - prevPos.y) * alpha};
}
