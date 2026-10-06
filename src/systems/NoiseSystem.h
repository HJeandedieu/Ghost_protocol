#pragma once

#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Events.h"
#include "entities/EnemySpec.h"
#include "entities/Weapon.h"

class EventBus;
struct Hearer {
    std::string id;
    Vec2 position;
};

class NoiseSystem {
   public:
    explicit NoiseSystem(EventBus& bus);
    void setWeapons(const std::vector<WeaponSpec>& weapons, const NoiseConfig& config);
    void setEnemies(const std::vector<EnemySpec>& enemies) { enemies_ = enemies; }
    void emit(Vec2 origin, float radius, NoiseType type, const std::string& sourceId);
    void setHearers(std::vector<Hearer> hearers);
    void setHearerPosition(std::size_t index, Vec2 position) {
        hearers_.at(index).position = position;
    }
    void beginTick() { radius_ = 0; }
    float currentRadius() const { return radius_; }

   private:
    void hear(const NoiseEmitted& event);
    EventBus& bus_;
    std::vector<Hearer> hearers_;
    float radius_ = 0;
    std::vector<WeaponSpec> weapons_;
    NoiseConfig config_;
    std::vector<EnemySpec> enemies_;
};
