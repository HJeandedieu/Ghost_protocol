#include "systems/AlarmDirector.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/EventBus.h"
#include "core/Logger.h"
#include "entities/Guard.h"
#include "world/World.h"

AlarmDirector::AlarmDirector(EventBus& events, Logger& logger, const std::vector<Guard>& guards)
    : events_(events), logger_(logger), guards_(guards) {
    events_.subscribe<CallInStarted>([this](const auto&) { update(); });
    events_.subscribe<CallInCancelled>([this](const auto&) { update(); });
    events_.subscribe<PagerMissed>([this](const auto&) { trigger(AlarmReason::Pager); });
    events_.subscribe<LaserTouched>([this](const auto& event) {
        if (event.count >= 2) trigger(AlarmReason::Laser);
    });
    events_.subscribe<NoiseEmitted>([this](const auto& event) {
        if (event.type != NoiseType::Shot || !std::isfinite(event.radius) || event.radius <= 0 ||
            !std::isfinite(event.origin.x) || !std::isfinite(event.origin.y))
            return;
        for (const auto& guard : guards_)
            if (!guard.dead() && guard.state() != GuardState::Unconscious &&
                guard.id != event.sourceId &&
                std::hypot(guard.pos.x - event.origin.x, guard.pos.y - event.origin.y) <=
                    event.radius) {
                trigger(AlarmReason::Shot);
                break;
            }
    });
}

void AlarmDirector::trigger(AlarmReason reason) {
    if (state_ == AlarmState::Loud) return;
    state_ = AlarmState::Loud;
    if (world_) {
        world_->alarmLoud = true;
        auto& map = world_->level.map;
        map.fillLight(LightLevel::Lit);
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x)
                if (map.tile(x, y) == TileType::FrontDoor) map.setOpen(x, y, true);
    }
    logger_.log(LogLevel::Info, "Alarm triggered");
    events_.publish(AlarmTriggered{reason});
}

AlarmDirector::AlarmDirector(EventBus& events, Logger& logger, World& world)
    : AlarmDirector(events, logger, world.guards) {
    cameras_ = &world.cameras;
    world_ = &world;
}

void AlarmDirector::update() {
    if (state_ == AlarmState::Loud) return;
    state_ = AlarmState::Quiet;
    for (const auto& guard : guards_) {
        if (guard.dead()) continue;
        if (guard.state() == GuardState::Combat) {
            trigger(AlarmReason::Combat);
            return;
        }
        if (guard.state() != GuardState::Alerted) continue;
        if (guard.callInRemaining() <= 0) {
            trigger(AlarmReason::CallIn);
            return;
        }
        state_ = AlarmState::CallIn;
    }
    if (cameras_)
        for (const auto& camera : *cameras_) {
            if (!camera.callingIn()) continue;
            if (camera.callInRemaining() <= 0) {
                trigger(AlarmReason::CallIn);
                return;
            }
            state_ = AlarmState::CallIn;
        }
}

float AlarmDirector::callInRemaining() const {
    float remaining = std::numeric_limits<float>::infinity();
    for (const auto& guard : guards_)
        if (!guard.dead() && guard.state() == GuardState::Alerted)
            remaining = std::min(remaining, guard.callInRemaining());
    if (cameras_)
        for (const auto& camera : *cameras_)
            if (camera.callingIn()) remaining = std::min(remaining, camera.callInRemaining());
    return std::isfinite(remaining) ? remaining : 0;
}
