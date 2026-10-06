#include "systems/RippleSystem.h"

#include <algorithm>
#include <cmath>

#include "entities/Entity.h"
#include "world/Raycast.h"
#include "world/TileMap.h"

RippleSystem::RippleSystem(const PingConfig& config, const TileMap& map)
    : config_(config),
      width_(map.width()),
      height_(map.height()),
      reveal_(static_cast<std::size_t>(width_) * height_, 0.0f) {}

void RippleSystem::startPing(Vec2 origin, float chargeSeconds) {
    if (cooldown_ > 0 || !std::isfinite(chargeSeconds)) return;
    origin_ = origin;
    const float interval = config_.chargeMax - config_.tapMax;
    const float charge =
        interval > 0 ? std::clamp((chargeSeconds - config_.tapMax) / interval, 0.0f, 1.0f) : 0.0f;
    maximum_ = config_.smallRadius + (config_.bigRadius - config_.smallRadius) * charge;
    radius_ = 0;
    cooldown_ = config_.cooldown;
    active_ = maximum_ > 0 && config_.speed > 0;
}

void RippleSystem::updateCharge(float dt, bool held, bool pressed, Vec2 origin) {
    if (!std::isfinite(dt) || dt <= 0) return;
    if (cooldown_ > 0) {
        charging_ = false;
        charge_ = 0;
        return;
    }
    if (pressed && !charging_) {
        charging_ = true;
        charge_ = 0;
    }
    if (!charging_) return;
    if (held)
        charge_ = std::min(config_.chargeMax, charge_ + dt);
    else {
        startPing(origin, charge_);
        charging_ = false;
        charge_ = 0;
    }
}

void RippleSystem::update(float dt, const TileMap& map, std::vector<Entity*>& entities) {
    if (!std::isfinite(dt) || dt <= 0 || map.width() != width_ || map.height() != height_) return;
    cooldown_ = std::max(0.0f, cooldown_ - dt);
    const float decay = config_.fade > 0 ? dt / config_.fade : 1.0f;
    for (auto& value : reveal_) value = std::max(0.0f, value - decay);
    for (auto* entity : entities)
        if (entity) entity->reveal = std::max(0.0f, entity->reveal - decay);
    if (!active_) return;
    const float previous = radius_;
    radius_ = std::min(maximum_, radius_ + config_.speed * dt);
    const float size = static_cast<float>(map.tileSize());
    const int minX = std::max(0, static_cast<int>(std::floor((origin_.x - radius_) / size)));
    const int maxX =
        std::min(width_ - 1, static_cast<int>(std::floor((origin_.x + radius_) / size)));
    const int minY = std::max(0, static_cast<int>(std::floor((origin_.y - radius_) / size)));
    const int maxY =
        std::min(height_ - 1, static_cast<int>(std::floor((origin_.y + radius_) / size)));
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x) {
            const auto center = map.tileCenter({x, y});
            const float distance = std::hypot(center.x - origin_.x, center.y - origin_.y);
            if (distance <= radius_ && (distance > previous || previous == 0) &&
                Raycast::hasLineOfSight(origin_, center, map, true)) {
                const float elapsed = dt - (distance - previous) / config_.speed;
                reveal_[static_cast<std::size_t>(y) * width_ + x] =
                    config_.fade > 0 ? std::max(0.0f, 1.0f - elapsed / config_.fade) : 0;
            }
        }
    for (auto* entity : entities)
        if (entity) {
            const float distance = std::hypot(entity->pos.x - origin_.x, entity->pos.y - origin_.y);
            if (distance <= radius_ && (distance > previous || previous == 0) &&
                Raycast::hasLineOfSight(origin_, entity->pos, map))
                entity->reveal =
                    config_.fade > 0
                        ? std::max(0.0f, 1.0f - (dt - (distance - previous) / config_.speed) /
                                                    config_.fade)
                        : 0;
        }
    if (radius_ >= maximum_) active_ = false;
}

float RippleSystem::tileReveal(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
    return reveal_[static_cast<std::size_t>(y) * width_ + x];
}

float RippleSystem::visibility(int x, int y, Vec2 player, const TileMap& map) const {
    if (!map.contains(x, y)) return 0;
    const auto center = map.tileCenter({x, y});
    if (std::hypot(center.x - player.x, center.y - player.y) <= config_.halo &&
        Raycast::hasLineOfSight(player, center, map, true))
        return 1;
    const auto light = map.light(x, y);
    return light == LightLevel::Lit
               ? 1.0f
               : std::max(tileReveal(x, y), light == LightLevel::Dim ? 0.35f : 0.0f);
}

float RippleSystem::cooldownFraction() const {
    return config_.cooldown > 0 ? 1.0f - std::clamp(cooldown_ / config_.cooldown, 0.0f, 1.0f)
                                : 1.0f;
}
