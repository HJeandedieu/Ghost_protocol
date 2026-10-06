#include "entities/Weapon.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <utility>

#include "core/Logger.h"

std::optional<std::vector<WeaponSpec>> loadWeapons(const std::string& path, Logger& logger) {
    try {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot open weapons file");
        nlohmann::json data;
        file >> data;
        const auto& entries = data.at("weapons");
        if (!entries.is_array()) throw std::runtime_error("weapons must be an array");
        std::vector<WeaponSpec> result;
        std::set<std::string> ids;
        const auto number = [](const auto& entry, const char* key, bool zero = false) {
            const auto& value = entry.at(key);
            if (!value.is_number()) throw std::runtime_error("Weapon field must be numeric");
            const float n = value.template get<float>();
            if (!std::isfinite(n) || (zero ? n < 0 : n <= 0))
                throw std::runtime_error("Invalid weapon number");
            return n;
        };
        const auto integer = [](const auto& entry, const char* key, bool zero = false) {
            const auto& value = entry.at(key);
            if (!value.is_number_integer())
                throw std::runtime_error("Weapon field must be integer");
            const auto n = value.template get<std::int64_t>();
            if (n > std::numeric_limits<int>::max() || (zero ? n < 0 : n <= 0))
                throw std::runtime_error("Invalid weapon count");
            return static_cast<int>(n);
        };
        for (const auto& entry : entries) {
            WeaponSpec spec;
            spec.id = entry.at("id").get<std::string>();
            spec.name = entry.at("name").get<std::string>();
            if ((spec.id != "whisper" && spec.id != "chatter" && spec.id != "gavel") ||
                spec.name.empty() || !ids.insert(spec.id).second)
                throw std::runtime_error("Invalid or duplicate weapon ID/name");
            spec.damage = number(entry, "damage");
            spec.pellets = integer(entry, "pellets");
            spec.magazine = integer(entry, "mag");
            spec.reserve = integer(entry, "reserve", true);
            spec.rate = number(entry, "rate");
            spec.reload = number(entry, "reload");
            spec.range = number(entry, "range");
            spec.spreadDeg = number(entry, "spread_deg", true);
            if (spec.spreadDeg > 360) throw std::runtime_error("Invalid weapon spread");
            const auto noise = entry.at("noise").get<std::string>();
            if (noise != "shot_suppressed" && noise != "shot")
                throw std::runtime_error("Unknown shot noise");
            spec.noise = noise == "shot_suppressed" ? NoiseType::ShotSupp : NoiseType::Shot;
            result.push_back(std::move(spec));
        }
        if (ids.size() != 3) throw std::runtime_error("Expected all three weapons");
        return result;
    } catch (const std::exception& error) {
        logger.log(LogLevel::Error, "Invalid weapons file: " + path + ": " + error.what());
        return std::nullopt;
    }
}
Weapon::Weapon(WeaponSpec spec)
    : spec_(std::move(spec)), ammunition_(spec_.magazine), reserve_(spec_.reserve) {}
void Weapon::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    cooldown_ = std::max(0.0f, cooldown_ - dt);
    if (reloadRemaining_ <= 0) return;
    reloadRemaining_ = std::max(0.0f, reloadRemaining_ - dt);
    if (reloadRemaining_ < 0.000001f) reloadRemaining_ = 0;
    if (reloadRemaining_ == 0) {
        const int transferred = std::min(spec_.magazine - ammunition_, reserve_);
        ammunition_ += transferred;
        reserve_ -= transferred;
    }
}
bool Weapon::beginReload() {
    if (reloadRemaining_ > 0 || ammunition_ == spec_.magazine || reserve_ == 0) return false;
    reloadRemaining_ = spec_.reload;
    return true;
}
bool Weapon::consumeShot() {
    if (reloadRemaining_ > 0 || cooldown_ > 0.000001f || ammunition_ <= 0) return false;
    --ammunition_;
    cooldown_ = 1.0f / spec_.rate;
    return true;
}
