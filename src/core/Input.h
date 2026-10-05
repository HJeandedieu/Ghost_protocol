#pragma once

// Frame edges are retained until a fixed tick consumes them.
struct Input {
    bool confirmPressed = false;
    bool startClicked = false;
    void clearEdges() {
        confirmPressed = false;
        startClicked = false;
    }
};
