#include "systems/LaserSystem.h"

#include <algorithm>
#include <cmath>

#include "core/EventBus.h"
#include "entities/Laser.h"
#include "entities/Player.h"
#include "world/Raycast.h"

LaserSystem::LaserSystem(EventBus& events, const LaserConfig& config, float noiseRadius)
    : events_(events), config_(config), noiseRadius_(noiseRadius) {
    events_.subscribe<SecurityLooped>([this](const auto&) { reset(); });
}

void LaserSystem::reset() {
    cooldown_ = window_ = 0;
    count_ = 0;
}

void LaserSystem::update(float dt, const Player& player, const std::vector<Laser>& lasers,
                         bool disabled) {
    if (!std::isfinite(dt) || dt <= 0) return;
    if (disabled) {
        reset();
        return;
    }
    cooldown_ = std::max(0.0, cooldown_ - dt);
    window_ -= dt;
    if (window_ < 0) count_ = 0;
    if (cooldown_ > 0 || !std::isfinite(player.pos.x) || !std::isfinite(player.pos.y)) return;
    for (const auto& laser : lasers) {
        auto point = laser.nearestPoint(player.pos);
        bool touching = std::hypot(point.x - player.pos.x, point.y - player.pos.y) <= player.radius;
        const float dy = player.pos.y - player.prevPos.y;
        if (!touching && dy != 0) {
            const float t = (laser.pos.y - player.prevPos.y) / dy;
            const float x = player.prevPos.x + (player.pos.x - player.prevPos.x) * t;
            if (t >= 0 && t <= 1 && x >= std::min(laser.pos.x, laser.end().x) &&
                x <= std::max(laser.pos.x, laser.end().x)) {
                touching = true;
                point = {x, laser.pos.y};
            }
        }
        if (!touching)
            for (const auto endpoint : {laser.pos, laser.end()})
                if (Raycast::intersectCircle(player.prevPos, player.pos, endpoint, player.radius)) {
                    touching = true;
                    point = endpoint;
                    break;
                }
        if (!touching) continue;
        ++count_;
        cooldown_ = config_.touchCooldown;
        if (count_ == 1) {
            window_ = config_.secondTouchWindow;
            events_.publish(NoiseEmitted{point, noiseRadius_, NoiseType::Laser, laser.id});
        }
        events_.publish(LaserTouched{laser.id, count_});
        break;
    }
}
