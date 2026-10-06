#pragma once

#include <memory>
#include <utility>

#include "entities/Guard.h"
#include "entities/Laser.h"
#include "entities/Player.h"
#include "entities/RecoveryPickup.h"
#include "entities/SecurityCamera.h"
#include "world/Level.h"

struct World {
    World(Level loaded, const PlayerConfig& config, const GuardConfig& guardConfig = {},
          const CameraConfig& cameraConfig = {})
        : level(std::move(loaded)), player(level.map.tileCenter(level.playerSpawn), config) {
        guards.reserve(level.guards.size());
        for (const auto& spawn : level.guards) guards.emplace_back(spawn, level.map, guardConfig);
        cameras.reserve(level.cameras.size());
        for (const auto& spawn : level.cameras)
            cameras.emplace_back(spawn, level.map, cameraConfig);
        lasers.reserve(level.lasers.size());
        for (const auto& spawn : level.lasers) lasers.emplace_back(spawn, level.map);
    }
    Level level;
    Player player;
    std::vector<Guard> guards;
    std::vector<SecurityCamera> cameras;
    std::vector<Laser> lasers;
    std::vector<std::unique_ptr<RecoveryPickup>> pickups;
    bool alarmLoud = false;
    bool powerOn = false;
    bool securityLoopUsed = false;
    float securityLoopRemaining = 0;
};
