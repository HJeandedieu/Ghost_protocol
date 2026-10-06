#pragma once

#include <utility>

#include "entities/Guard.h"
#include "entities/Player.h"
#include "world/Level.h"

struct World {
    World(Level loaded, const PlayerConfig& config, const GuardConfig& guardConfig = {})
        : level(std::move(loaded)), player(level.map.tileCenter(level.playerSpawn), config) {
        guards.reserve(level.guards.size());
        for (const auto& spawn : level.guards) guards.emplace_back(spawn, level.map, guardConfig);
    }
    Level level;
    Player player;
    std::vector<Guard> guards;
    bool powerOn = false;
    bool securityLoopUsed = false;
    float securityLoopRemaining = 0;
};
