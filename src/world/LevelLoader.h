#pragma once

#include <optional>
#include <string>

#include "world/Level.h"

class Logger;

class LevelLoader {
   public:
    static std::optional<Level> load(const std::string& path, Logger& logger);
};
