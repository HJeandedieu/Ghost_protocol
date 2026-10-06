#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/Events.h"

class Logger;
struct WeaponSpec {
    std::string id;
    std::string name;
    float damage = 0;
    int pellets = 0;
    int magazine = 0;
    int reserve = 0;
    float rate = 0;
    float reload = 0;
    float range = 0;
    float spreadDeg = 0;
    NoiseType noise = NoiseType::Shot;
};
std::optional<std::vector<WeaponSpec>> loadWeapons(const std::string& path, Logger& logger);

class Weapon {
   public:
    explicit Weapon(WeaponSpec spec);
    void update(float dt);
    bool beginReload();
    bool consumeShot();
    const WeaponSpec& spec() const { return spec_; }
    int ammunition() const { return ammunition_; }
    int reserve() const { return reserve_; }
    float reloadRemaining() const { return reloadRemaining_; }

   private:
    WeaponSpec spec_;
    int ammunition_;
    int reserve_;
    float cooldown_ = 0;
    float reloadRemaining_ = 0;
};
