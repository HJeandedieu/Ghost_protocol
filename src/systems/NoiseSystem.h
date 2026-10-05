#pragma once

#include <string>
#include <vector>

#include "core/Events.h"

class EventBus;
struct Hearer {
    std::string id;
    Vec2 position;
};

class NoiseSystem {
   public:
    explicit NoiseSystem(EventBus& bus);
    void emit(Vec2 origin, float radius, NoiseType type, const std::string& sourceId);
    void setHearers(std::vector<Hearer> hearers);
    void beginTick() { radius_ = 0; }
    float currentRadius() const { return radius_; }

   private:
    void hear(const NoiseEmitted& event);
    EventBus& bus_;
    std::vector<Hearer> hearers_;
    float radius_ = 0;
};
