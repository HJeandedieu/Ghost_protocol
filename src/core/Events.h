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
