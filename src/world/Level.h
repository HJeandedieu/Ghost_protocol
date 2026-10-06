#pragma once

#include <string>
#include <vector>

#include "world/TileMap.h"

enum class PatrolMode { Loop, PingPong, Stationary };

struct GuardSpawn {
    std::string id;
    std::string room;
    PatrolMode mode = PatrolMode::Loop;
    bool pager = false;
    std::vector<TileCoord> waypoints;
    float facing = 0.0f;
};

struct CameraSpawn {
    std::string id;
    std::string room;
    TileCoord position;
    float angle = 0.0f;
    float sweep = 0.0f;
    float range = 0.0f;
};

struct LaserSpawn {
    std::string id;
    TileCoord a;
    TileCoord b;
};

struct Level {
    std::string name;
    TileMap map;
    TileCoord playerSpawn;
    std::vector<GuardSpawn> guards;
    std::vector<CameraSpawn> cameras;
    std::vector<LaserSpawn> lasers;
};
