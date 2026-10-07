#pragma once
#include "core/Input.h"

// A UI click must be released before it can become a gameplay shot.
class GameplayInputGate {
   public:
    void blockFire() { blocked_ = true; }
    Input filter(const Input& input) {
        if (!input.fireHeld) blocked_ = false;
        Input result = input;
        if (blocked_) result.fireHeld = result.firePressed = false;
        return result;
    }

   private:
    bool blocked_ = true;
};
