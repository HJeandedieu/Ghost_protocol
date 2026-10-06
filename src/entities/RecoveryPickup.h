#pragma once
#include <utility>

#include "entities/Entity.h"

enum class PickupType { Medkit, ArmorPlate };

class RecoveryPickup : public Entity {
   public:
    RecoveryPickup(std::string pickupId, PickupType pickupType, Vec2 position) : type(pickupType) {
        id = std::move(pickupId);
        pos = prevPos = position;
    }
    PickupType type;
};
