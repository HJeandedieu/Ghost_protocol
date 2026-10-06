#include "world/Pathfinder.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

#include "world/Raycast.h"
#include "world/TileMap.h"

Pathfinder::Pathfinder(const GuardConfig& config, const TileMap& map) : config_(config) {
    prepare(map);
}

void Pathfinder::prepare(const TileMap& map) const {
    const auto count = static_cast<std::size_t>(map.width()) * map.height();
    cost_.resize(count);
    parent_.resize(count);
    open_.reserve(count * 8 + 1);
}

std::vector<Vec2> Pathfinder::findPath(Vec2 from, Vec2 to, const TileMap& map) const {
    std::vector<Vec2> path;
    findPath(from, to, map, path);
    return path;
}

void Pathfinder::findPath(Vec2 from, Vec2 to, const TileMap& map, std::vector<Vec2>& path) const {
    path.clear();
    const auto clear = [&](Vec2 a, Vec2 b) {
        return Raycast::isPathClear(a, b, config_.radius, map, config_.pathClearStep);
    };
    if (!clear(from, from) || !clear(to, to)) return;
    if (clear(from, to)) {
        path.push_back(to);
        return;
    }
    const int width = map.width();
    const auto index = [&](Vec2 point) {
        return static_cast<int>(point.y / map.tileSize()) * width +
               static_cast<int>(point.x / map.tileSize());
    };
    const int start = index(from), goal = index(to);
    if (start == goal) return;
    const auto position = [&](int cell) {
        return cell == start ? from
                             : (cell == goal ? to : map.tileCenter({cell % width, cell / width}));
    };
    const auto distance = [](Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); };
    prepare(map);
    auto& cost = cost_;
    auto& parent = parent_;
    auto& open = open_;
    std::fill(cost.begin(), cost.end(), std::numeric_limits<float>::infinity());
    std::fill(parent.begin(), parent.end(), -1);
    open.clear();
    using Entry = std::pair<float, int>;
    const auto enqueue = [&](float estimate, int cell) {
        open.emplace_back(estimate, cell);
        std::push_heap(open.begin(), open.end(), std::greater<Entry>{});
    };
    cost[start] = 0;
    enqueue(distance(from, to), start);
    while (!open.empty()) {
        std::pop_heap(open.begin(), open.end(), std::greater<Entry>{});
        const auto [estimate, current] = open.back();
        open.pop_back();
        const Vec2 point = position(current);
        if (estimate > cost[current] + distance(point, to)) continue;
        if (current == goal) {
            for (int cell = goal; cell != start; cell = parent[cell])
                path.push_back(position(cell));
            std::reverse(path.begin(), path.end());
            return;
        }
        const int x = current % width, y = current / width;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if ((dx == 0 && dy == 0) || !map.contains(x + dx, y + dy)) continue;
                const int next = (y + dy) * width + x + dx;
                const Vec2 destination = position(next);
                if (!clear(point, destination)) continue;
                const float candidate = cost[current] + distance(point, destination);
                if (candidate >= cost[next]) continue;
                cost[next] = candidate;
                parent[next] = current;
                enqueue(candidate + distance(destination, to), next);
            }
    }
}
