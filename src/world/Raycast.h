#pragma once

#include <optional>

#include "core/Config.h"
#include "core/Vec2.h"
#include "core/Vec3.h"

class TileMap;

class Raycast {
   public:
    static std::optional<float> intersectPrism(ShotRay ray, Vec3 minimum, Vec3 maximum);
    static std::optional<float> intersectCylinder(ShotRay ray, Vec2 center, float radius,
                                                  float height);
    static std::optional<float> intersectFloor(ShotRay ray);
    static std::optional<float> intersectShield(ShotRay ray, Vec2 center, float facing,
                                                const ShotGeometryConfig& geometry);
    static std::optional<float> blockingDistance(ShotRay ray, float range, const TileMap& map,
                                                 float wallHeight);
    static bool hasLineOfSight(Vec2 from, Vec2 to, const TileMap& map,
                               bool revealBlockingTarget = false);
    static float sightDistance(Vec2 from, Vec2 to, const TileMap& map);
    static bool isPathClear(Vec2 from, Vec2 to, float radius, const TileMap& map,
                            float step = GuardConfig{}.pathClearStep);
    static std::optional<float> intersectCircle(Vec2 from, Vec2 to, Vec2 center, float radius);
};
