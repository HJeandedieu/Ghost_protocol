#pragma once

#include "entities/Entity.h"

enum class BagState { Stack, Carried, Dropped, Delivered };
enum class DyeState { Unarmed, Armed, Disarmed, Spoiled };

struct Bag : Entity {
    BagState state = BagState::Stack;
    DyeState dye = DyeState::Unarmed;
    float value = 0;
    float burstAge = 0;
};
