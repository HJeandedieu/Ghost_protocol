#pragma once
#include <deque>
#include <map>
#include <memory>

#include "core/Config.h"
#include "entities/EnemySpec.h"
#include "world/TileMap.h"
class EventBus;
struct World;
struct SpawnView {
    float left = 0, top = 0, right = 0, bottom = 0;
};
class WaveSpawner {
   public:
    WaveSpawner(EventBus& events, World& world, const std::vector<EnemySpec>& enemies,
                const std::vector<WaveSpec>& waves, std::map<std::string, TileCoord> entries,
                const AlarmConfig& config);
    void update(float dt, SpawnView view);
    int waveIndex() const { return waveIndex_; }
    std::size_t pendingCount() const { return pending_.size(); }
    float nextWaveRemaining() const;

   private:
    struct EntryRotation {
        std::vector<std::string> points;
        std::size_t cursor = 0;
    };
    struct Pending {
        EnemySpec spec;
        std::shared_ptr<EntryRotation> entries;
    };
    EventBus& events_;
    World& world_;
    std::vector<EnemySpec> enemies_;
    std::vector<WaveSpec> waves_;
    std::map<std::string, TileCoord> entries_;
    AlarmConfig config_;
    std::deque<Pending> pending_;
    double elapsed_ = 0, nextWave_ = 0;
    int waveIndex_ = 0;
    std::size_t nextId_ = 0;
    bool active_ = false;
    void acceptWave();
    void spawnPending(SpawnView view);
};
