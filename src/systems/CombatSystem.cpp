#include "systems/CombatSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "core/EventBus.h"
#include "world/Raycast.h"
#include "world/World.h"
namespace {
constexpr float kRadians = 3.14159265358979323846f / 180.0f;
WeaponSpec selected(const std::vector<WeaponSpec>& specs, const std::string& id) {
    const auto entry =
        std::find_if(specs.begin(), specs.end(), [id](const auto& spec) { return spec.id == id; });
    if (entry == specs.end()) throw std::invalid_argument("Missing default weapon");
    return *entry;
}
}  // namespace
CombatSystem::CombatSystem(EventBus& events, const std::vector<WeaponSpec>& specs,
                           std::uint32_t seed, std::array<std::string, 2> loadout,
                           ShotGeometryConfig geometry, float wallHeight)
    : events_(events),
      weapons_{Weapon(selected(specs, loadout[0])), Weapon(selected(specs, loadout[1]))},
      rng_(seed),
      geometry_(geometry),
      wallHeight_(wallHeight) {
    if (loadout[0] == loadout[1]) throw std::invalid_argument("Loadout needs two distinct weapons");
}
void CombatSystem::update(float dt, const Input& input, ShotRay ray, World& world) {
    if (!std::isfinite(dt) || dt <= 0) return;
    updateHealth(dt, world.player);
    if (world.player.dead()) return;
    shotAge_ += dt;
    for (auto& weapon : weapons_) weapon.update(dt);
    if (input.weaponSlot >= 0 && input.weaponSlot < 2) activeSlot_ = input.weaponSlot;
    if (input.weaponWheel % 2 != 0) activeSlot_ = 1 - activeSlot_;
    auto& weapon = weapons_[activeSlot_];
    if (input.reloadPressed) weapon.beginReload();
    const auto aim = ray.normalized();
    if (!(input.firePressed || input.fireHeld) || !input.mouseInViewport || !aim ||
        !weapon.consumeShot())
        return;
    lastShot_ = fire(weapon, *aim, rng_, world);
    shotAge_ = 0;
    events_.publish(ShotFired{world.player.id, weapon.spec().id, aim->origin.planar(),
                              aim->direction.planar()});
}
HitResult CombatSystem::fire(const Weapon& weapon, ShotRay aim, Rng& rng, World& world) {
    HitResult result;
    const auto unit = aim.normalized();
    if (!unit) return result;
    aim = *unit;
    const auto& spec = weapon.spec();
    const auto horizontal = Vec3{0, 1, 0}.cross(aim.direction).normalized();
    const Vec3 right = horizontal ? *horizontal : *Vec3{1, 0, 0}.cross(aim.direction).normalized();
    const Vec3 up = aim.direction.cross(right);
    result.pellets.reserve(static_cast<std::size_t>(spec.pellets));
    for (int i = 0; i < spec.pellets; ++i) {
        // Both samples are consumed even at zero spread, as required by the contract.
        const float upper = std::nextafter(1.f, 0.f);
        const double u = std::min(rng.uniformFloat(0, 1), upper);
        const double v = std::min(rng.uniformFloat(0, 1), upper);
        const double cosine = 1 - u * (1 - std::cos(spec.spreadDeg * .5 * kRadians));
        const double sine = std::sqrt(std::max(0.0, 1 - cosine * cosine));
        const double azimuth = 2 * 3.14159265358979323846 * v;
        const Vec3 offset = right * static_cast<float>(sine * std::cos(azimuth)) +
                            up * static_cast<float>(sine * std::sin(azimuth));
        const ShotRay ray{aim.origin,
                          *(aim.direction * static_cast<float>(cosine) + offset).normalized()};
        const auto block = Raycast::blockingDistance(ray, spec.range, world.level.map, wallHeight_);
        float nearest = block.value_or(spec.range);
        float impactDistance = nearest;
        Entity* victim = nullptr;
        bool shieldHit = false;
        ShotImpact impact = block ? ShotImpact::Geometry : ShotImpact::None;
        const auto consider = [&](Entity& entity, float height, const Enemy* enemy) {
            const auto body = Raycast::intersectCylinder(ray, entity.pos, entity.radius, height);
            const bool hasShield = enemy && enemy->spec().id == "shield_cop";
            const auto plate =
                hasShield ? Raycast::intersectShield(ray, entity.pos, enemy->facing(), geometry_)
                          : std::optional<float>{};
            if (!body && !plate) return;
            const float infinity = std::numeric_limits<float>::infinity();
            const float entry = std::min(body.value_or(infinity), plate.value_or(infinity));
            if (entry > spec.range || (block && entry >= *block) || entry > nearest ||
                (victim && entry == nearest && entity.id >= victim->id))
                return;
            nearest = entry;
            victim = &entity;
            shieldHit = plate && (!body || *plate <= *body);
            if (plate && body && *plate <= spec.range && (!block || *plate < *block)) {
                const float bodyHeight = ray.at(*body).y;
                const bool covered = bodyHeight >= geometry_.shieldBottom &&
                                     bodyHeight <= geometry_.shieldBottom + geometry_.shieldHeight;
                // The plate can sit just inside its owner's body cylinder.
                if (covered && enemy->shieldFaces(ray.origin.planar())) shieldHit = true;
            }
            impactDistance = shieldHit ? *plate : *body;
            impact = shieldHit ? ShotImpact::Shield : ShotImpact::Body;
        };
        for (auto& guard : world.guards)
            if (!guard.dead() && guard.state() != GuardState::Unconscious)
                consider(guard, geometry_.guardHeight, nullptr);
        for (auto& enemy : world.enemies)
            if (!enemy->dead())
                consider(*enemy, geometry_.enemyHeight(enemy->spec().id), enemy.get());
        float damage = 0;
        if (victim) {
            damage = spec.damage;
            if (shieldHit)
                damage =
                    static_cast<Enemy*>(victim)->shieldPlateDamage(damage, ray.origin.planar());
            applyDamage(*victim, damage, world.player.id);
        }
        const auto end = ray.at(impactDistance);
        const float yaw = std::atan2(ray.direction.z, ray.direction.x) / kRadians;
        result.pellets.push_back({ray.origin.planar(), end.planar(), yaw, victim ? victim->id : "",
                                  damage, ray.origin, end, ray.direction, impact});
    }
    return result;
}

void CombatSystem::applyDamage(Entity& target, float amount, const std::string& sourceId) {
    if (!std::isfinite(amount) || amount <= 0 || target.maximumHp_ <= 0 || target.dead()) return;
    const float absorbed = std::min(target.armor_, amount);
    target.armor_ -= absorbed;
    target.hp_ = std::max(0.0f, target.hp_ - (amount - absorbed));
    target.secondsSinceDamage_ = 0;
    events_.publish(EntityDamaged{target.id, amount, sourceId});
    if (target.dead()) {
        events_.publish(EntityDied{target.id});
        if (dynamic_cast<Player*>(&target)) events_.publish(PlayerDowned{});
    }
}
void CombatSystem::updateHealth(float dt, Player& player) {
    if (!std::isfinite(dt) || dt <= 0 || player.dead()) return;
    const auto& config = player.healthConfig();
    const double before = player.secondsSinceDamage_;
    player.secondsSinceDamage_ += dt;
    const double active = std::max(0.0, player.secondsSinceDamage_ - config.armorRegenDelay) -
                          std::max(0.0, before - config.armorRegenDelay);
    player.armor_ = std::min(player.maximumArmor_,
                             player.armor_ + static_cast<float>(active) * config.armorRegen);
}

float CombatSystem::restore(Player& player, PickupType type, float amount) {
    if (!std::isfinite(amount) || amount <= 0 || player.dead()) return 0;
    float& value = type == PickupType::Medkit ? player.hp_ : player.armor_;
    const float maximum = type == PickupType::Medkit ? player.maximumHp_ : player.maximumArmor_;
    const float before = value;
    value = std::min(maximum, value + amount);
    return value - before;
}
