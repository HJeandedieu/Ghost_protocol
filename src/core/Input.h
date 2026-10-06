#pragma once
#include "core/Vec2.h"

// Frame edges are retained until a fixed tick consumes them.
struct Input {
    bool confirmPressed = false;
    bool startClicked = false;
    bool debugPressed = false;
    bool crouchPressed = false;
    bool sprintHeld = false;
    Vec2 move;
    Vec2 mouseLogical;
    bool mouseInViewport = false;
    bool pingPressed = false;
    bool pingHeld = false;
    bool interactHeld = false;
    bool interactPressed = false;
    void clearEdges() {
        confirmPressed = false;
        startClicked = false;
        debugPressed = false;
        crouchPressed = false;
        pingPressed = false;
        interactPressed = false;
    }
};
