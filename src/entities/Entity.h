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
    float hp() const { return hp_; }
    float armor() const { return armor_; }
    float maximumHp() const { return maximumHp_; }
    float maximumArmor() const { return maximumArmor_; }
    bool dead() const { return maximumHp_ > 0 && hp_ <= 0; }
    float reveal = 0.0f;  // Written only by RippleSystem.
   protected:
    void initializeVitals(float hp, float armor) {
        hp_ = maximumHp_ = hp;
        armor_ = maximumArmor_ = armor;
    }

   private:
    friend class CombatSystem;
    float hp_ = 0;
    float armor_ = 0;
    float maximumHp_ = 0;
    float maximumArmor_ = 0;
    double secondsSinceDamage_ = 0;
};
