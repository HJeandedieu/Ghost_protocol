#pragma once

#include <string>

#include "core/Vec2.h"

class Entity {
   public:
    virtual ~Entity() = default;
    virtual Vec2 nearestPoint(Vec2) const { return pos; }
    Vec2 pos;
    Vec2 prevPos;
    float radius = 0.0f;
    std::string id;
    float reveal = 0.0f;  // Written only by RippleSystem.
};
