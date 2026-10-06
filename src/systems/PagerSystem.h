#pragma once

#include <string>
#include <vector>

#include "core/Config.h"

class EventBus;
class Guard;
class InteractionSystem;
struct World;

enum class PagerState { Inactive, Waiting, Ringing, Answered, Missed };

class PagerSystem {
   public:
    PagerSystem(EventBus& events, const PagerConfig& config, World& world,
                InteractionSystem& interaction);
    void update(float dt);
    PagerState state(const std::string& bodyId) const;
    float remaining(const std::string& bodyId) const;

   private:
    struct Pager {
        std::string id;
        PagerState state = PagerState::Inactive;
        double elapsed = 0;
    };
    EventBus& events_;
    const PagerConfig config_;
    std::vector<Pager> pagers_;
};
