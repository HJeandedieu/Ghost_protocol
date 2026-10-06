#include "systems/DetectionSystem.h"

#include <algorithm>
#include <cmath>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "systems/VisionSystem.h"

DetectionSystem::DetectionSystem(EventBus& events, Logger& logger, std::vector<Guard>& guards,
                                 float difficultyFill)
    : events_(events), logger_(logger), difficultyFill_(difficultyFill) {
    events_.subscribe<BodyFound>([this, &guards](const BodyFound& event) {
        const auto body = std::find_if(guards.begin(), guards.end(), [&](const Guard& guard) {
            return guard.id == event.bodyId && guard.state() == GuardState::Unconscious;
        });
        if (body == guards.end()) return;
        for (auto& guard : guards) {
            if (guard.id != event.guardId || guard.state_ == GuardState::Unconscious ||
                guard.state_ == GuardState::Combat || guard.state_ == GuardState::Alerted)
                continue;
            startCallIn(guard);
            break;
        }
    });
}

void DetectionSystem::startCallIn(Guard& guard) {
    guard.state_ = GuardState::Alerted;
    guard.detection_ = 100;
    guard.callInRemaining_ = guard.visionConfig().callin;
    guard.callInCompleted_ = false;
    events_.publish(CallInStarted{guard.id, guard.callInRemaining_});
    logger_.log(LogLevel::Info, guard.id + ": call-in started");
}

void DetectionSystem::advanceCallIn(float dt, Guard& guard) {
    if (guard.callInCompleted_) return;
    guard.callInRemaining_ = std::max(0.0f, guard.callInRemaining_ - dt);
    if (guard.callInRemaining_ == 0) {
        guard.callInCompleted_ = true;
        // Day 10 stub: AlarmDirector will own the alarm transition in Day 13.
        logger_.log(LogLevel::Info, guard.id + ": call-in completed");
    }
}

void DetectionSystem::update(float dt, const Player& player, const TileMap& map,
                             std::vector<Guard>& guards) {
    if (!std::isfinite(dt) || dt <= 0) return;
    for (auto& guard : guards) {
        if (guard.state_ == GuardState::Alerted) {
            advanceCallIn(dt, guard);
            continue;
        }
        if (guard.state_ == GuardState::Combat || guard.state_ == GuardState::Unconscious) continue;
        const auto& config = guard.visionConfig();
        const VisionSystem vision(config);
        if (vision.sees(guard, player, map)) {
            const float distance =
                std::hypot(player.pos.x - guard.pos.x, player.pos.y - guard.pos.y);
            const int tx = static_cast<int>(std::floor(player.pos.x / map.tileSize()));
            const int ty = static_cast<int>(std::floor(player.pos.y / map.tileSize()));
            const float range = vision.rangeFor(map.light(tx, ty), player.isCrouched());
            const float fraction = range > 0 ? std::clamp(distance / range, 0.0f, 1.0f) : 0;
            const float movement = player.isCrouched()
                                       ? config.crouchMult
                                       : (player.isSprinting() ? config.sprintMult : 1.0f);
            const float rate = (config.fillNear + (config.fillFar - config.fillNear) * fraction) *
                               movement * difficultyFill_;
            if (!std::isfinite(rate) || rate <= 0) continue;
            if (guard.state_ != GuardState::Suspicious) {
                guard.beginSuspicion(player.pos, false);
                events_.publish(GuardSuspicious{guard.id, player.pos});
            }
            if (!guard.noiseInterest_) guard.suspiciousElapsed_ += dt;
            guard.facing_ = std::atan2(player.pos.y - guard.pos.y, player.pos.x - guard.pos.x);
            const float timeToSpot = (100.0f - guard.detection_) / rate;
            guard.detection_ = std::min(100.0f, guard.detection_ + rate * dt);
            if (guard.detection_ >= 100) {
                events_.publish(GuardSpotted{guard.id});
                startCallIn(guard);
                // Only time after the meter filled belongs to the call-in.
                advanceCallIn(std::max(0.0f, dt - timeToSpot), guard);
            }
        } else {
            guard.detection_ = std::max(0.0f, guard.detection_ - config.decay * dt);
            if (guard.state_ == GuardState::Suspicious && !guard.noiseInterest_) {
                guard.suspiciousElapsed_ += dt;
                if (guard.detection_ == 0 && guard.suspiciousElapsed_ >= config.suspiciousTime)
                    guard.returnToRoute();
            }
        }
    }
}
