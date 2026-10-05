#include "render/Renderer.h"

#include "raylib.h"

void Renderer::drawFoundation(float alpha) {
    // Entity interpolation will consume alpha when movement is introduced.
    (void)alpha;
    constexpr Color kInk = {10, 10, 12, 255};
    BeginDrawing();
    ClearBackground(kInk);
    DrawFPS(16, 16);
    EndDrawing();
}
