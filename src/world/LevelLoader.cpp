#include "world/LevelLoader.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

#include "core/Logger.h"

namespace {
using Json = nlohmann::json;

int integer(const Json& value) {
    if (!value.is_number_integer()) throw std::runtime_error("Expected integer");
    const auto wide = value.get<std::int64_t>();
    if (wide < std::numeric_limits<int>::min() || wide > std::numeric_limits<int>::max())
        throw std::runtime_error("Integer outside supported range");
    return static_cast<int>(wide);
}

float number(const Json& value) {
    if (!value.is_number()) throw std::runtime_error("Expected number");
    const float result = value.get<float>();
    if (!std::isfinite(result)) throw std::runtime_error("Expected finite number");
    return result;
}

TileCoord coordinate(const Json& value, const TileMap& map) {
    if (!value.is_array() || value.size() != 2) throw std::runtime_error("Invalid tile coordinate");
    TileCoord result{integer(value.at(0)), integer(value.at(1))};
    if (!map.contains(result.x, result.y) || map.tile(result.x, result.y) == TileType::Wall)
        throw std::runtime_error("Entity coordinate is outside walkable geometry");
    return result;
}

LightLevel lightLevel(const Json& value) {
    const auto name = value.get<std::string>();
    if (name == "dark") return LightLevel::Dark;
    if (name == "dim") return LightLevel::Dim;
    if (name == "lit") return LightLevel::Lit;
    throw std::runtime_error("Unknown light level");
}

const Json& array(const Json& data, const char* key) {
    const auto& result = data.at(key);
    if (!result.is_array()) throw std::runtime_error(std::string(key) + " must be an array");
    return result;
}
}  // namespace

std::optional<Level> LevelLoader::load(const std::string& path, Logger& logger) {
    try {
        std::ifstream metadata(path);
        if (!metadata) throw std::runtime_error("Cannot open level metadata");
        Json data;
        metadata >> data;
        const auto mapFile = data.at("map_file").get<std::string>();
        if (mapFile.empty() || mapFile == "." || mapFile == ".." ||
            mapFile.find_first_of("/\\:") != std::string::npos)
            throw std::runtime_error("map_file must be a sibling filename");
        const auto slash = path.find_last_of("/\\");
        const auto directory =
            slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
        std::ifstream mapSource(directory + mapFile);
        if (!mapSource) throw std::runtime_error("Cannot open level map");
        Level level;
        level.name = data.at("name").get<std::string>();
        level.map = TileMap::parse(mapSource, integer(data.at("tile_size")));
        if (level.map.width() != integer(data.at("width")) ||
            level.map.height() != integer(data.at("height")))
            throw std::runtime_error("Map dimensions differ from metadata");
        int spawns = 0;
        for (int y = 0; y < level.map.height(); ++y)
            for (int x = 0; x < level.map.width(); ++x)
                if (level.map.tile(x, y) == TileType::PlayerSpawn) {
                    level.playerSpawn = {x, y};
                    ++spawns;
                }
        if (spawns != 1) throw std::runtime_error("Level needs exactly one player spawn");
        level.map.fillLight(lightLevel(data.at("default_light")));
        for (const auto& zone : array(data, "light_zones")) {
            zone.at("name").get<std::string>();
            const auto& rect = zone.at("rect");
            if (!rect.is_array() || rect.size() != 4)
                throw std::runtime_error("Invalid light rectangle");
            const int x0 = integer(rect.at(0)), y0 = integer(rect.at(1));
            const int x1 = integer(rect.at(2)), y1 = integer(rect.at(3));
            if (!level.map.contains(x0, y0) || !level.map.contains(x1, y1) || x1 < x0 || y1 < y0)
                throw std::runtime_error("Light rectangle outside map");
            const auto light = lightLevel(zone.at("level"));
            if (light == LightLevel::Dark)
                throw std::runtime_error("Light zone must be dim or lit");
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) level.map.setLight(x, y, light);
        }
        std::set<std::string> ids;
        auto readId = [&ids](const Json& item) {
            auto id = item.at("id").get<std::string>();
            if (id.empty() || !ids.insert(id).second)
                throw std::runtime_error("Empty or duplicate entity ID");
            return id;
        };
        for (const auto& item : array(data, "guards")) {
            GuardSpawn guard;
            guard.id = readId(item);
            guard.room = item.at("room").get<std::string>();
            if (item.at("type") != "patrol_guard") throw std::runtime_error("Unknown guard type");
            const auto mode = item.at("mode").get<std::string>();
            if (mode == "loop")
                guard.mode = PatrolMode::Loop;
            else if (mode == "pingpong")
                guard.mode = PatrolMode::PingPong;
            else if (mode == "stationary")
                guard.mode = PatrolMode::Stationary;
            else
                throw std::runtime_error("Unknown patrol mode");
            guard.pager = item.at("pager").get<bool>();
            for (const auto& wp : array(item, "wp"))
                guard.waypoints.push_back(coordinate(wp, level.map));
            if (guard.waypoints.empty()) throw std::runtime_error("Guard has no waypoints");
            if (item.contains("facing")) guard.facing = number(item.at("facing"));
            level.guards.push_back(std::move(guard));
        }
        for (const auto& item : array(data, "cameras")) {
            CameraSpawn camera;
            camera.id = readId(item);
            camera.room = item.at("room").get<std::string>();
            camera.position = coordinate(item.at("pos"), level.map);
            camera.angle = number(item.at("angle"));
            camera.sweep = number(item.at("sweep"));
            camera.range = number(item.at("range_px"));
            if (camera.sweep < 0 || camera.range <= 0)
                throw std::runtime_error("Invalid camera sweep or range");
            level.cameras.push_back(std::move(camera));
        }
        for (const auto& item : array(data, "lasers")) {
            LaserSpawn laser{readId(item), coordinate(item.at("a"), level.map),
                             coordinate(item.at("b"), level.map)};
            if (laser.a.y != laser.b.y) throw std::runtime_error("Laser must be horizontal");
            level.lasers.push_back(std::move(laser));
        }
        logger.log(LogLevel::Info, "Loaded level: " + level.name);
        return level;
    } catch (const std::exception& error) {
        logger.log(LogLevel::Error, "Invalid level " + path + ": " + error.what());
        return std::nullopt;
    }
}
