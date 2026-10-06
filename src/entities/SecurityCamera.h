#pragma once

#include "core/Config.h"
#include "entities/Entity.h"
#include "world/Level.h"

class SecurityCamera : public Entity {
   public:
    SecurityCamera(const CameraSpawn& spawn, const TileMap& map, const CameraConfig& config);
    void update(float dt);
    float facing() const { return facing_; }
    float range() const { return range_; }
    float detection() const { return detection_; }
    bool callingIn() const { return callingIn_; }
    float callInRemaining() const { return callInRemaining_; }
    float coneDegrees() const { return config_.coneDeg; }

   private:
    friend class DetectionSystem;
    const CameraConfig config_;
    float center_ = 0;
    float sweep_ = 0;
    double phase_ = 0;
    float facing_ = 0;
    float range_ = 0;
    float detection_ = 0;
    bool callingIn_ = false;
    float callInRemaining_ = 0;
};
