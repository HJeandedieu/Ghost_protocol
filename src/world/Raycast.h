#pragma once

#include <optional>

#include "core/Vec2.h"

class TileMap;

class Raycast {
   public:
    static bool hasLineOfSight(Vec2 from, Vec2 to, const TileMap& map,
                               bool revealBlockingTarget = false);
    static std::optional<float> intersectCircle(Vec2 from, Vec2 to, Vec2 center, float radius);
};
