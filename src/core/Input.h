#pragma once
#include "core/Vec2.h"

// Frame edges are retained until a fixed tick consumes them.
struct Input {
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
    bool takedownPressed = false;
    bool firePressed = false;
    bool fireHeld = false;
    bool reloadPressed = false;
    int weaponSlot = -1;
    int weaponWheel = 0;
    void clearEdges() {
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
        takedownPressed = false;
        firePressed = false;
        reloadPressed = false;
        weaponSlot = -1;
        weaponWheel = 0;
    }
};
