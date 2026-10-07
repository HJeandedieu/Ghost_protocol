#pragma once
#include "core/Vec2.h"

// Frame edges are retained until a fixed tick consumes them.
struct Input {
    bool backPressed = false;
    int menuVertical = 0;
    int menuHorizontal = 0;
    bool confirmPressed = false;
    bool startClicked = false;
    bool debugPressed = false;
    bool debugDamagePressed = false;
    bool debugMedkitPressed = false;
    bool debugArmorPressed = false;
    bool debugCopPressed = false;
    bool crouchPressed = false;
    bool sprintHeld = false;
    Vec2 move;
    Vec2 mouseLogical;
    bool mouseInViewport = false;
    bool pingPressed = false;
    bool pingHeld = false;
    bool interactHeld = false;
    bool interactPressed = false;
    bool throwPressed = false;
    bool takedownPressed = false;
    bool firePressed = false;
    bool fireHeld = false;
    bool reloadPressed = false;
    int weaponSlot = -1;
    int loadoutExcluded = -1;
    int weaponWheel = 0;
    void clearEdges() {
        backPressed = false;
        menuVertical = 0;
        menuHorizontal = 0;
        confirmPressed = false;
        startClicked = false;
        debugPressed = false;
        debugDamagePressed = false;
        debugMedkitPressed = false;
        debugArmorPressed = false;
        debugCopPressed = false;
        crouchPressed = false;
        pingPressed = false;
        interactPressed = false;
        throwPressed = false;
        takedownPressed = false;
        firePressed = false;
        reloadPressed = false;
        weaponSlot = -1;
        loadoutExcluded = -1;
        weaponWheel = 0;
    }
};

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
