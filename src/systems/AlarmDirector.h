#pragma once

#include <vector>

#include "core/Events.h"

class EventBus;
class Logger;
class Guard;
class SecurityCamera;
struct World;

enum class AlarmState { Quiet, CallIn, Loud };

class AlarmDirector {
   public:
    AlarmDirector(EventBus& events, Logger& logger, const std::vector<Guard>& guards);
    AlarmDirector(EventBus& events, Logger& logger, World& world);
    void update();
    void trigger(AlarmReason reason);
    AlarmState state() const { return state_; }
    float callInRemaining() const;

   private:
    EventBus& events_;
    Logger& logger_;
    const std::vector<Guard>& guards_;
    const std::vector<SecurityCamera>* cameras_ = nullptr;
    World* world_ = nullptr;
    AlarmState state_ = AlarmState::Quiet;
};
