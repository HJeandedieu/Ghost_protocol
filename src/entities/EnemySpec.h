#pragma once

#include <optional>
#include <string>
#include <vector>

class Logger;

struct EnemySpec {
    std::string id;
    float hp = 0;
    float armor = 0;
    float speed = 0;
    float damage = 0;
    float rate = 0;
    float accuracy = 0;
    float engage = 0;
    int burst = 1;
    float shieldArcDeg = 0;
    float shieldBlock = 0;
};

std::optional<std::vector<EnemySpec>> loadEnemies(const std::string& path, Logger& logger);
