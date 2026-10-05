#pragma once

#include "core/Config.h"
#include "core/Vec2.h"

class FollowCamera {
   public:
    FollowCamera(Vec2 spawn, const ViewConfig& config);
    void update(float dt, Vec2 player, Vec2 cursorOffset);
    Vec2 target() const { return target_; }
    Vec2 interpolatedTarget(float alpha) const;

   private:
    const ViewConfig config_;
    Vec2 target_;
    Vec2 previous_;
};
