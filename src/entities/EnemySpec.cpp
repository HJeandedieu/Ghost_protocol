#include "entities/EnemySpec.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <utility>

#include "core/Logger.h"

std::optional<std::vector<EnemySpec>> loadEnemies(const std::string& path, Logger& logger) {
    try {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot open enemies file");
        nlohmann::json data;
        file >> data;
        const auto& entries = data.at("enemies");
        if (!entries.is_array()) throw std::runtime_error("enemies must be an array");
        const auto number = [](const auto& entry, const char* key, bool zero = false) {
            const auto& value = entry.at(key);
            if (!value.is_number()) throw std::runtime_error("Enemy field must be numeric");
            const float n = value.template get<float>();
            if (!std::isfinite(n) || (zero ? n < 0 : n <= 0))
                throw std::runtime_error("Invalid enemy number");
            return n;
        };
        std::vector<EnemySpec> result;
        std::set<std::string> ids;
        for (const auto& entry : entries) {
            EnemySpec spec;
            spec.id = entry.at("id").get<std::string>();
            if ((spec.id != "patrol_guard" && spec.id != "cop" && spec.id != "shield_cop" &&
                 spec.id != "heavy") ||
                !ids.insert(spec.id).second)
                throw std::runtime_error("Invalid or duplicate enemy ID");
            spec.hp = number(entry, "hp");
            spec.armor = number(entry, "armor", true);
            spec.speed = number(entry, "speed");
            spec.damage = number(entry, "dmg");
            spec.rate = number(entry, "rate");
            spec.accuracy = number(entry, "accuracy", true);
            if (spec.accuracy > 1) throw std::runtime_error("Invalid enemy accuracy");
            spec.engage = number(entry, "engage");
            if (spec.id != "patrol_guard") spec.radius = number(entry, "radius");
            if (entry.contains("burst")) {
                const auto& value = entry.at("burst");
                if (!value.is_number_integer())
                    throw std::runtime_error("Enemy burst must be an integer");
                const auto count = value.template get<std::int64_t>();
                if (count <= 0 || count > std::numeric_limits<int>::max())
                    throw std::runtime_error("Invalid enemy burst");
                spec.burst = static_cast<int>(count);
            }
            if (spec.id == "shield_cop") {
                spec.shieldArcDeg = number(entry, "shield_arc_deg");
                spec.shieldBlock = number(entry, "shield_block", true);
                if (spec.shieldArcDeg > 360 || spec.shieldBlock > 1)
                    throw std::runtime_error("Invalid shield tuning");
            }
            result.push_back(std::move(spec));
        }
        if (ids.size() != 4) throw std::runtime_error("Expected all four enemy types");
        return result;
    } catch (const std::exception& error) {
        logger.log(LogLevel::Error, "Invalid enemies file: " + path + ": " + error.what());
        return std::nullopt;
    }
}

std::optional<std::vector<WaveSpec>> loadWaves(const std::string& path, Logger& logger) {
    try {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot open wave file");
        nlohmann::json data;
        file >> data;
        const auto& entries = data.at("waves");
        if (!entries.is_array() || entries.size() != 6)
            throw std::runtime_error("Expected five waves and repeat");
        std::vector<WaveSpec> result;
        std::set<int> indices;
        const std::array<std::string, 3> types{"cop", "shield_cop", "heavy"};
        for (const auto& entry : entries) {
            WaveSpec wave;
            const auto& at = entry.at("at");
            if (at.is_string() && at == "repeat")
                wave.index = -1;
            else {
                if (!at.is_number_integer()) throw std::runtime_error("Invalid wave index");
                const auto index = at.get<std::int64_t>();
                if (index < 0 || index > 4) throw std::runtime_error("Invalid wave index");
                wave.index = static_cast<int>(index);
            }
            if (!indices.insert(wave.index).second)
                throw std::runtime_error("Duplicate wave index");
            const auto& spawn = entry.at("spawn");
            if (!spawn.is_object() || spawn.empty())
                throw std::runtime_error("Invalid wave composition");
            for (auto item = spawn.begin(); item != spawn.end(); ++item) {
                const auto type = std::find(types.begin(), types.end(), item.key());
                if (type == types.end() || !item.value().is_number_integer())
                    throw std::runtime_error("Invalid wave enemy/count");
                const auto count = item.value().get<std::int64_t>();
                if (count <= 0 || count > std::numeric_limits<int>::max())
                    throw std::runtime_error("Invalid wave count");
                wave.counts[static_cast<std::size_t>(type - types.begin())] =
                    static_cast<int>(count);
            }
            const auto& points = entry.at("points");
            if (!points.is_array() || points.empty())
                throw std::runtime_error("Missing wave entries");
            for (const auto& point : points) {
                const auto name = point.get<std::string>();
                if (name != "front" && name != "service" && name != "east")
                    throw std::runtime_error("Unknown wave entry");
                wave.points.push_back(name);
            }
            result.push_back(std::move(wave));
        }
        return result;
    } catch (const std::exception& error) {
        logger.log(LogLevel::Error, "Invalid waves file: " + path + ": " + error.what());
        return std::nullopt;
    }
}
