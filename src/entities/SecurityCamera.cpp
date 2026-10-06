#include "entities/SecurityCamera.h"

#include <cmath>

SecurityCamera::SecurityCamera(const CameraSpawn& spawn, const TileMap& map,
                               const CameraConfig& config)
    : config_(config),
      center_(spawn.angle),
      sweep_(spawn.sweep),
      phase_(spawn.sweep),
      facing_(spawn.angle),
      range_(spawn.range > 0 ? spawn.range : config.range) {
    id = spawn.id;
    pos = prevPos = map.tileCenter(spawn.position);
}

void SecurityCamera::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0 || sweep_ <= 0) return;
    phase_ = std::fmod(phase_ + config_.sweepSpeed * dt, 4.0 * sweep_);
    facing_ = center_ + (phase_ <= 2 * sweep_ ? phase_ - sweep_ : 3 * sweep_ - phase_);
}
