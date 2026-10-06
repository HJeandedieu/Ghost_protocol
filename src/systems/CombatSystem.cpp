#include "systems/CombatSystem.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "core/EventBus.h"
#include "world/Raycast.h"
#include "world/World.h"
namespace {
constexpr float kRadians = 3.14159265358979323846f / 180.0f;
WeaponSpec selected(const std::vector<WeaponSpec>& specs, const char* id) {
    const auto entry =
        std::find_if(specs.begin(), specs.end(), [id](const auto& spec) { return spec.id == id; });
    if (entry == specs.end()) throw std::invalid_argument("Missing default weapon");
    return *entry;
}
}  // namespace
CombatSystem::CombatSystem(EventBus& events, const std::vector<WeaponSpec>& specs,
                           std::uint32_t seed)
    : events_(events),
      weapons_{Weapon(selected(specs, "whisper")), Weapon(selected(specs, "chatter"))},
      rng_(seed) {}
void CombatSystem::update(float dt, const Input& input, float dirDeg, World& world) {
    if (!std::isfinite(dt) || dt <= 0) return;
    shotAge_ += dt;
    for (auto& weapon : weapons_) weapon.update(dt);
    if (input.weaponSlot >= 0 && input.weaponSlot < 2) activeSlot_ = input.weaponSlot;
    if (input.weaponWheel % 2 != 0) activeSlot_ = 1 - activeSlot_;
    auto& weapon = weapons_[activeSlot_];
    if (input.reloadPressed) weapon.beginReload();
    if (!(input.firePressed || input.fireHeld) || !input.mouseInViewport ||
        !std::isfinite(dirDeg) || !weapon.consumeShot())
        return;
    lastShot_ = fire(weapon, world.player.pos, dirDeg, rng_, world);
    shotAge_ = 0;
    events_.publish(ShotFired{world.player.id,
                              weapon.spec().id,
                              world.player.pos,
                              {std::cos(dirDeg * kRadians), std::sin(dirDeg * kRadians)}});
}
HitResult CombatSystem::fire(const Weapon& weapon, Vec2 from, float dirDeg, Rng& rng,
                             World& world) {
    HitResult result;
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(dirDeg)) return result;
    const auto& spec = weapon.spec();
    result.pellets.reserve(static_cast<std::size_t>(spec.pellets));
    for (int i = 0; i < spec.pellets; ++i) {
        const float angle =
            dirDeg + rng.uniformFloat(-spec.spreadDeg * 0.5f, spec.spreadDeg * 0.5f);
        const Vec2 direction{std::cos(angle * kRadians), std::sin(angle * kRadians)};
        const Vec2 maximum{from.x + direction.x * spec.range, from.y + direction.y * spec.range};
        float distance = Raycast::sightDistance(from, maximum, world.level.map);
        Vec2 end{from.x + direction.x * distance, from.y + direction.y * distance};
        std::string target;
        for (const auto& guard : world.guards) {
            if (guard.state() == GuardState::Unconscious) continue;
            const auto hit = Raycast::intersectCircle(from, end, guard.pos, guard.radius);
            if (!hit) continue;
            distance *= *hit;
            end = {from.x + direction.x * distance, from.y + direction.y * distance};
            target = guard.id;
        }
        result.pellets.push_back({from, end, angle, target, spec.damage});
    }
    return result;
}
