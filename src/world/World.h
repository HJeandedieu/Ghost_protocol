#pragma once

#include <utility>

#include "entities/Player.h"
#include "world/Level.h"

struct World {
    World(Level loaded, const PlayerConfig& config)
        : level(std::move(loaded)), player(level.map.tileCenter(level.playerSpawn), config) {}
    Level level;
    Player player;
    bool powerOn = false;
    bool securityLoopUsed = false;
    float securityLoopRemaining = 0;
};
