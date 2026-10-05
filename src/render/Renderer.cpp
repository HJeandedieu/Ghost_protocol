#include "render/Renderer.h"

#include "raylib.h"
#include "render/Letterbox.h"

Renderer::Renderer() : surface_(LoadRenderTexture(Letterbox::kWidth, Letterbox::kHeight)) {}
Renderer::~Renderer() { UnloadRenderTexture(surface_); }

void Renderer::beginFrame() const {
    constexpr Color kInk = {10, 10, 12, 255};
    BeginTextureMode(surface_);
    ClearBackground(kInk);
}

void Renderer::present() const {
    DrawFPS(16, 16);
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    const auto viewport = Letterbox::fit(GetScreenWidth(), GetScreenHeight());
    if (viewport.width > 0.0f && viewport.height > 0.0f) {
        DrawTexturePro(
            surface_.texture,
            {0, 0, static_cast<float>(Letterbox::kWidth), -static_cast<float>(Letterbox::kHeight)},
            {viewport.x, viewport.y, viewport.width, viewport.height}, {0, 0}, 0, WHITE);
    }
    EndDrawing();
}

void Renderer::drawPlaceholder(const char* title, const char* subtitle) {
    constexpr Color kBone = {233, 228, 208, 255};
    constexpr Color kTeal = {63, 143, 140, 255};
    DrawText(title, (Letterbox::kWidth - MeasureText(title, 40)) / 2, 288, 40, kBone);
    DrawText(subtitle, (Letterbox::kWidth - MeasureText(subtitle, 20)) / 2, 360, 20, kTeal);
    DrawText("F11 fullscreen  |  ESC quit", 24, Letterbox::kHeight - 40, 18, kBone);
}
