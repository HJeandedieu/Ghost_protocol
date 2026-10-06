#pragma once

#include "entities/Entity.h"
#include "world/Level.h"

class Laser : public Entity {
   public:
    Laser(const LaserSpawn& spawn, const TileMap& map);
    Vec2 nearestPoint(Vec2 point) const override;
    Vec2 end() const { return end_; }

   private:
    Vec2 end_{};
};
