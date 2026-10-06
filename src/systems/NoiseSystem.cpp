#include "systems/NoiseSystem.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/EventBus.h"

NoiseSystem::NoiseSystem(EventBus& bus) : bus_(bus) {
    bus_.subscribe<ShotFired>([this](const ShotFired& event) {
        const auto weapon = std::find_if(weapons_.begin(), weapons_.end(), [&](const auto& entry) {
            return entry.id == event.weaponId;
        });
        if (weapon != weapons_.end())
            emit(event.from,
                 weapon->noise == NoiseType::ShotSupp ? config_.shotSuppressed : config_.shot,
                 weapon->noise, event.shooterId);
    });
    bus_.subscribe<NoiseEmitted>([this](const NoiseEmitted& event) { hear(event); });
}
void NoiseSystem::setHearers(std::vector<Hearer> hearers) { hearers_ = std::move(hearers); }
void NoiseSystem::emit(Vec2 origin, float radius, NoiseType type, const std::string& sourceId) {
    if (std::isfinite(radius) && radius > 0)
        bus_.publish(NoiseEmitted{origin, radius, type, sourceId});
}
void NoiseSystem::hear(const NoiseEmitted& event) {
    if (!std::isfinite(event.radius) || event.radius <= 0) return;
    radius_ = std::max(radius_, event.radius);
    for (const auto& hearer : hearers_) {
        if (hearer.id != event.sourceId &&
            std::hypot(hearer.position.x - event.origin.x, hearer.position.y - event.origin.y) <=
                event.radius)
            bus_.publish(GuardSuspicious{hearer.id, event.origin});
    }
}

void NoiseSystem::setWeapons(const std::vector<WeaponSpec>& weapons, const NoiseConfig& config) {
    weapons_ = weapons;
    config_ = config;
}
