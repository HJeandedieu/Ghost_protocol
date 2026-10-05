#pragma once

#include <string>

#include "core/Vec2.h"

class Entity {
   public:
    virtual ~Entity() = default;
    Vec2 pos;
    Vec2 prevPos;
    float radius = 0.0f;
    std::string id;
};
