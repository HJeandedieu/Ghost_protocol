#include "systems/WaveSpawner.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "core/EventBus.h"
#include "world/Raycast.h"
#include "world/World.h"
namespace {
bool fits(Vec2 pos, float radius, const TileMap& map) {
    if (!Raycast::isPathClear(pos, pos, radius, map, radius)) return false;
    const float size = static_cast<float>(map.tileSize());
    for (int y = static_cast<int>((pos.y - radius) / size);
         y <= static_cast<int>((pos.y + radius) / size); ++y)
        for (int x = static_cast<int>((pos.x - radius) / size);
             x <= static_cast<int>((pos.x + radius) / size); ++x) {
            if (map.isPassable(x, y)) continue;
            const float dx = pos.x - std::clamp(pos.x, x * size, (x + 1) * size);
            const float dy = pos.y - std::clamp(pos.y, y * size, (y + 1) * size);
            if (dx * dx + dy * dy < radius * radius) return false;
        }
    return true;
}
}  // namespace
WaveSpawner::WaveSpawner(EventBus& events, World& world, const std::vector<EnemySpec>& enemies,
                         const std::vector<WaveSpec>& waves,
                         std::map<std::string, TileCoord> entries, const AlarmConfig& config)
    : events_(events),
      world_(world),
      enemies_(enemies),
      waves_(waves),
      entries_(std::move(entries)),
      config_(config),
      nextWave_(config.firstWaveDelay) {
    if (!std::isfinite(config.waveInterval) || config.waveInterval <= 0)
        throw std::invalid_argument("Wave interval must be positive");
}
float WaveSpawner::nextWaveRemaining() const {
    return static_cast<float>(std::max(0.0, nextWave_ - elapsed_));
}
void WaveSpawner::acceptWave() {
    const int index = waveIndex_ < 5 ? waveIndex_ : -1;
    const auto wave = std::find_if(waves_.begin(), waves_.end(),
                                   [&](const auto& entry) { return entry.index == index; });
    if (wave == waves_.end()) throw std::logic_error("Missing wave composition");
    const auto alive = std::count_if(world_.enemies.begin(), world_.enemies.end(),
                                     [](const auto& enemy) { return !enemy->dead(); });
    const auto occupied = static_cast<std::size_t>(alive) + pending_.size();
    const auto cap = static_cast<std::size_t>(std::max(0.f, config_.maxAlive));
    std::size_t remaining = occupied < cap ? cap - occupied : 0;
    const std::array<std::string, 3> types{"cop", "shield_cop", "heavy"};
    auto rotation = std::make_shared<EntryRotation>(EntryRotation{wave->points, 0});
    for (std::size_t type = 0; type < types.size() && remaining > 0; ++type) {
        const auto spec = std::find_if(enemies_.begin(), enemies_.end(),
                                       [&](const auto& entry) { return entry.id == types[type]; });
        if (spec == enemies_.end()) throw std::logic_error("Missing police spec");
        for (int count = 0; count < wave->counts[type] && remaining > 0; ++count, --remaining) {
            pending_.push_back({*spec, rotation});
        }
    }
    ++waveIndex_;
    events_.publish(WaveSpawned{waveIndex_});
}
void WaveSpawner::spawnPending(SpawnView view) {
    auto& map = world_.level.map;
    for (auto item = pending_.begin(); item != pending_.end();) {
        bool spawned = false;
        auto& rotation = *item->entries;
        for (std::size_t offset = 0; offset < rotation.points.size(); ++offset) {
            const auto entryIndex = (rotation.cursor + offset) % rotation.points.size();
            const auto point = entries_.find(rotation.points[entryIndex]);
            if (point == entries_.end()) throw std::logic_error("Missing entry coordinate");
            const Vec2 pos = map.tileCenter(point->second);
            const float radius = item->spec.radius;
            const bool visible = view.intersects(pos, radius);
            if (visible || !map.isPassable(point->second.x, point->second.y) ||
                !fits(pos, radius, map))
                continue;
            const std::string id = "police:" + std::to_string(nextId_++);
            if (item->spec.id == "cop")
                world_.enemies.push_back(std::make_unique<Cop>(id, pos, item->spec, radius));
            else if (item->spec.id == "shield_cop")
                world_.enemies.push_back(std::make_unique<ShieldCop>(id, pos, item->spec));
            else
                world_.enemies.push_back(std::make_unique<Heavy>(id, pos, item->spec));
            spawned = true;
            rotation.cursor = (entryIndex + 1) % rotation.points.size();
            break;
        }
        if (spawned)
            item = pending_.erase(item);
        else
            ++item;
    }
}
void WaveSpawner::update(float dt, SpawnView view) {
    if (!std::isfinite(dt) || dt <= 0 || (!active_ && !world_.alarmLoud)) return;
    active_ = true;
    elapsed_ += dt;
    while (elapsed_ + 1e-6 >= nextWave_) {
        acceptWave();
        nextWave_ += config_.waveInterval;
    }
    spawnPending(view);
}
