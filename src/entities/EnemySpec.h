#pragma once

#include <array>
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
    float radius = 0;
    int burst = 1;
    float shieldArcDeg = 0;
    float shieldBlock = 0;
};

std::optional<std::vector<EnemySpec>> loadEnemies(const std::string& path, Logger& logger);

struct WaveSpec {
    int index = 0;                // -1 is the repeating composition.
    std::array<int, 3> counts{};  // cop, shield_cop, heavy.
    std::vector<std::string> points;
};
std::optional<std::vector<WaveSpec>> loadWaves(const std::string& path, Logger& logger);
