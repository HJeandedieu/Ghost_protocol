#pragma once

#include <utility>
#include <vector>

#include "core/Config.h"
#include "core/Vec2.h"

class TileMap;

class Pathfinder {
   public:
    explicit Pathfinder(const GuardConfig& config = {}) : config_(config) {}
    Pathfinder(const GuardConfig& config, const TileMap& map);
    std::vector<Vec2> findPath(Vec2 from, Vec2 to, const TileMap& map) const;
    void findPath(Vec2 from, Vec2 to, const TileMap& map, std::vector<Vec2>& path) const;

   private:
    const GuardConfig config_;
    mutable std::vector<float> cost_;
    mutable std::vector<int> parent_;
    mutable std::vector<std::pair<float, int>> open_;
    void prepare(const TileMap& map) const;
};
