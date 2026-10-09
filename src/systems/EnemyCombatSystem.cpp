#include "systems/EnemyCombatSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/EventBus.h"
#include "systems/CombatSystem.h"
#include "systems/PickupSystem.h"
#include "world/Pathfinder.h"
#include "world/Raycast.h"
#include "world/World.h"
namespace {
float distance(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
}  // namespace
EnemyCombatSystem::EnemyCombatSystem(EventBus& events, World& world, CombatSystem& combat,
                                     PickupSystem& pickups, const std::vector<EnemySpec>& specs,
                                     const Config& config, std::uint32_t seed,
                                     float damageMultiplier)
    : events_(events),
      world_(world),
      combat_(combat),
      config_(config),
      damageMultiplier_(damageMultiplier >= 0 ? damageMultiplier
                                              : config.difficulty.normal.enemyDmg),
      specs_(specs),
      rng_(seed) {
    const auto guardSpec = std::find_if(specs_.begin(), specs_.end(),
                                        [](const auto& s) { return s.id == "patrol_guard"; });
    if (guardSpec != specs_.end())
        for (auto& guard : world_.guards)
            if (guard.maximumHp() == 0) guard.initializeVitals(guardSpec->hp, guardSpec->armor);
    events_.subscribe<EntityDied>([this, &pickups](const auto& event) {
        for (const auto& enemy : world_.enemies)
            if (enemy->id == event.targetId && enemy->dead())
                pickups.dropForPolice(enemy->id, enemy->spec().id, enemy->pos, world_);
    });
}
bool EnemyCombatSystem::spawnDebugCop() {
    const auto spec =
        std::find_if(specs_.begin(), specs_.end(), [](const auto& s) { return s.id == "cop"; });
    if (spec == specs_.end()) return false;
    const auto& map = world_.level.map;
    Vec2 chosen{};
    float best = std::numeric_limits<float>::infinity();
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const Vec2 point = map.tileCenter({x, y});
            const float d = distance(point, world_.player.pos);
            if (d > spec->engage || d < world_.player.radius * 4 || d >= best ||
                !map.isPassable(x, y) ||
                !Raycast::isPathClear(world_.player.pos, point, config_.guard.radius, map,
                                      config_.guard.pathClearStep))
                continue;
            chosen = point;
            best = d;
        }
    if (!std::isfinite(best)) return false;
    world_.enemies.push_back(
        std::make_unique<Cop>("cop:" + std::to_string(nextCop_++), chosen, *spec, spec->radius));
    return true;
}
void EnemyCombatSystem::moveToward(Entity& entity, Actor& actor, float speed, float dt) {
    actor.pathTime -= dt;
    if (actor.pathTime <= 0) {
        auto movement = config_.guard;
        movement.radius = entity.radius;
        actor.path = Pathfinder(movement).findPath(entity.pos, world_.player.pos, world_.level.map);
        actor.waypoint = 0;
        actor.pathTime = config_.enemyCombat.pathRefresh;
    }
    float budget = speed * dt;
    while (actor.waypoint < actor.path.size() && budget > 0) {
        const Vec2 goal = actor.path[actor.waypoint];
        const float length = distance(entity.pos, goal);
        if (length <= config_.guard.arriveTolerance) {
            ++actor.waypoint;
            continue;
        }
        const float step = std::min(budget, length);
        entity.pos = world_.level.map.moveCircle(
            entity.pos,
            {(goal.x - entity.pos.x) / length * step, (goal.y - entity.pos.y) / length * step},
            entity.radius);
        budget -= step;
        if (step < length) break;
    }
}
void EnemyCombatSystem::updateActor(Entity& entity, float& facing, const EnemySpec& spec,
                                    float speed, bool strafe, float dt) {
    auto& actor = actors_[entity.id];
    entity.prevPos = entity.pos;
    const auto canShoot = [&] {
        return !world_.player.dead() && distance(entity.pos, world_.player.pos) <= spec.engage &&
               Raycast::hasLineOfSight(entity.pos, world_.player.pos, world_.level.map);
    };
    bool engaged = canShoot();
    if (!engaged) {
        actor.acquired = false;
        actor.elapsed = 0;
        actor.bullet = 0;
        actor.strafeTime = 0;
        moveToward(entity, actor, speed, dt);
    } else if (spec.id == "shield_cop") {
        const float length = distance(entity.pos, world_.player.pos);
        if (length > 0) {
            const float step = std::min(speed * dt, length);
            entity.pos =
                world_.level.map.moveCircle(entity.pos,
                                            {(world_.player.pos.x - entity.pos.x) / length * step,
                                             (world_.player.pos.y - entity.pos.y) / length * step},
                                            entity.radius);
        }
    } else if (strafe) {
        const float length = distance(entity.pos, world_.player.pos);
        if (length > 0) {
            const float sign =
                static_cast<long long>(actor.strafeTime / config_.enemyCombat.strafeReverseTime) %
                            2 ==
                        0
                    ? 1.f
                    : -1.f;
            const Vec2 delta{world_.player.pos.x - entity.pos.x,
                             world_.player.pos.y - entity.pos.y};
            entity.pos = world_.level.map.moveCircle(
                entity.pos,
                {-delta.y / length * sign * config_.enemyCombat.strafeSpeed * dt,
                 delta.x / length * sign * config_.enemyCombat.strafeSpeed * dt},
                entity.radius);
        }
        actor.strafeTime += dt;
    }
    facing = std::atan2(world_.player.pos.y - entity.pos.y, world_.player.pos.x - entity.pos.x);
    const bool opportunity = engaged && canShoot();
    if (auto* enemy = dynamic_cast<Enemy*>(&entity))
        enemy->state_ = opportunity ? EnemyState::Engage : EnemyState::Advance;
    if (!opportunity) {
        actor.acquired = false;
        actor.elapsed = 0;
        actor.bullet = 0;
        return;
    }
    if (!actor.acquired) {
        actor.acquired = true;
        actor.elapsed = 0;
        actor.bullet = 0;
        actor.nextBullet = actor.burstStart = config_.enemyCombat.reactionTime;
    }
    actor.elapsed += dt;
    while (actor.elapsed + 1e-6 >= actor.nextBullet && !world_.player.dead()) {
        const bool hit = rng_.uniformFloat(0, 1) < spec.accuracy;
        Vec2 endpoint = world_.player.pos;
        if (!hit) {
            const float length = distance(entity.pos, endpoint);
            if (length > 0) {
                endpoint.x +=
                    -(world_.player.pos.y - entity.pos.y) / length * world_.player.radius * 2;
                endpoint.y +=
                    (world_.player.pos.x - entity.pos.x) / length * world_.player.radius * 2;
            }
        }
        const float playerHeight = world_.player.isCrouched()
                                       ? config_.shotGeometry.playerCrouchHeight
                                       : config_.shotGeometry.playerHeight;
        const Vec3 origin{entity.pos.x, config_.shotGeometry.enemyHeight(spec.id) * .5f,
                          entity.pos.y};
        const Vec3 target{endpoint.x, playerHeight * .5f, endpoint.y};
        const auto direction = (target - origin).normalized();
        bool reachedPlayer = false;
        Vec3 impact = origin;
        if (direction) {
            const ShotRay ray{origin, *direction};
            const auto block = Raycast::blockingDistance(ray, spec.engage, world_.level.map,
                                                         config_.view.wallHeight);
            const auto body = Raycast::intersectCylinder(ray, world_.player.pos,
                                                         world_.player.radius, playerHeight);
            reachedPlayer = hit && body && *body <= spec.engage && (!block || *body < *block);
            impact = ray.at(reachedPlayer ? *body : block.value_or(spec.engage));
            shots_.push_back({entity.pos, impact.planar(), 0, origin, impact, reachedPlayer});
            events_.publish(ShotFired{entity.id, spec.id, entity.pos, direction->planar()});
            if (reachedPlayer)
                combat_.applyDamage(world_.player, spec.damage * damageMultiplier_, entity.id);
        }
        ++actor.bullet;
        if (actor.bullet >= spec.burst) {
            actor.bullet = 0;
            actor.burstStart += 1.0 / spec.rate;
            actor.nextBullet = actor.burstStart;
        } else
            actor.nextBullet += config_.enemyCombat.burstInterval;
    }
}
void EnemyCombatSystem::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    for (auto& shot : shots_) shot.age += dt;
    shots_.erase(
        std::remove_if(shots_.begin(), shots_.end(), [](const auto& s) { return s.age > 0.08f; }),
        shots_.end());
    const auto fade = [&](Entity& entity) {
        entity.deathOpacity_ =
            std::max(0.f, entity.deathOpacity_ - dt / config_.enemyCombat.deathFade);
    };
    for (auto& enemy : world_.enemies) {
        if (enemy->dead()) {
            fade(*enemy);
            continue;
        }
        updateActor(*enemy, enemy->facing_, enemy->spec(), enemy->spec().speed,
                    enemy->spec().id == "cop", dt);
    }
    const auto spec = std::find_if(specs_.begin(), specs_.end(),
                                   [](const auto& s) { return s.id == "patrol_guard"; });
    if (spec != specs_.end())
        for (auto& guard : world_.guards) {
            if (guard.dead()) {
                fade(guard);
                continue;
            }
            if (guard.state() == GuardState::Combat)
                updateActor(guard, guard.facing_, *spec, config_.guard.chaseSpeed, false, dt);
        }
}
