#pragma once

#include <optional>

#include "core/Config.h"
#include "core/Vec2.h"

class TileMap;

class Raycast {
   public:
    static bool hasLineOfSight(Vec2 from, Vec2 to, const TileMap& map,
                               bool revealBlockingTarget = false);
    static float sightDistance(Vec2 from, Vec2 to, const TileMap& map);
    static bool isPathClear(Vec2 from, Vec2 to, float radius, const TileMap& map,
                            float step = GuardConfig{}.pathClearStep);
    static std::optional<float> intersectCircle(Vec2 from, Vec2 to, Vec2 center, float radius);
};
