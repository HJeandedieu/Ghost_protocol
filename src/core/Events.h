#pragma once

#include <string>

#include "core/Vec2.h"

enum class NoiseType { Step, Ping, Lockpick, Crack, ShotSupp, Shot, Laser };
struct NoiseEmitted {
    Vec2 origin;
    float radius = 0;
    NoiseType type = NoiseType::Step;
    std::string sourceId;
};
struct GuardSuspicious {
    std::string guardId;
    Vec2 point;
};
struct GuardSpotted {
    std::string guardId;
};
struct CallInStarted {
    std::string guardId;
    float seconds = 0;
};
struct CallInCancelled {
    std::string guardId;
};
struct GuardTakenDown {
    std::string guardId;
    bool hasPager = false;
};
struct BodyFound {
    std::string guardId;
    std::string bodyId;
};
struct PagerRang {
    std::string bodyId;
};
struct PagerAnswered {
    std::string bodyId;
};
struct PagerMissed {
    std::string bodyId;
};
struct LaserTouched {
    std::string laserId;
    int count = 0;
};
enum class AlarmReason { CallIn, Laser, Pager, Shot, Thermite, Combat };
struct AlarmTriggered {
    AlarmReason reason = AlarmReason::CallIn;
};
struct InteractionProgress {
    std::string interactableId;
    float progress = 0;
};
struct InteractionDone {
    std::string interactableId;
};
struct SecurityLooped {
    float secondsLeft = 0;
};
