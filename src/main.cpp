#include "raylib.h"

int main() {
    constexpr int kScreenWidth = 1280;
    constexpr int kScreenHeight = 720;
    constexpr Color kInk = {10, 10, 12, 255};

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(kScreenWidth, kScreenHeight, "Ghost Protocol");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(kInk);
        DrawFPS(16, 16);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
