#pragma once

// Frame edges are retained until a fixed tick consumes them.
struct Input {
    bool confirmPressed = false;
    bool startClicked = false;
    bool debugPressed = false;
    void clearEdges() {
        confirmPressed = false;
        startClicked = false;
        debugPressed = false;
    }
};
