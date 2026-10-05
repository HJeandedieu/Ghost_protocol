#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "entities/Player.h"
#include "raylib.h"
#include "render/Letterbox.h"
#include "world/Level.h"

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

void Renderer::drawError(const char* message) {
    DrawRectangle(24, 424, Letterbox::kWidth - 48, 56, {20, 22, 27, 255});
    DrawText(message, 40, 440, 20, {255, 59, 92, 255});
}

void Renderer::drawLevel(const Level& level, const Player& player, Vec2 cameraTarget, float facing,
                         float alpha, bool overview, std::uint32_t seed) {
    constexpr Color kBone = {233, 228, 208, 255};
    constexpr Color kTeal = {63, 143, 140, 255};
    constexpr Color kDeepTeal = {30, 74, 74, 255};
    constexpr Color kGold = {242, 183, 5, 255};
    const auto& map = level.map;
    const float size = static_cast<float>(map.tileSize());
    const auto spawn = player.interpolatedPosition(alpha);
    Camera2D camera{};
    camera.offset = {Letterbox::kWidth * 0.5f, Letterbox::kHeight * 0.5f};
    camera.target = {cameraTarget.x, cameraTarget.y};
    camera.zoom = 1.0f;
    if (overview) {
        camera.target = {map.width() * size * 0.5f, map.height() * size * 0.5f};
        camera.zoom = std::min((Letterbox::kWidth - 48.0f) / (map.width() * size),
                               (Letterbox::kHeight - 128.0f) / (map.height() * size));
    }
    BeginMode2D(camera);
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const auto tile = map.tile(x, y);
            DrawRectangleRec({x * size, y * size, size, size},
                             tile == TileType::Wall ? kTeal : kDeepTeal);
            if (tile != TileType::Wall && tile != TileType::Floor) {
                const auto center = map.tileCenter({x, y});
                const auto color = map.isPassable(x, y) ? kGold : kBone;
                DrawRectangleRec(
                    {center.x - size * 0.2f, center.y - size * 0.2f, size * 0.4f, size * 0.4f},
                    color);
                if (overview) {
                    const char symbol[] = {static_cast<char>(tile), '\0'};
                    DrawText(symbol, static_cast<int>(x * size + size * 0.25f),
                             static_cast<int>(y * size + size * 0.25f),
                             static_cast<int>(size * 0.5f), {10, 10, 12, 255});
                }
            }
        }
    }
    DrawCircleV({spawn.x, spawn.y}, player.radius, {10, 10, 12, 255});
    DrawCircleV({spawn.x, spawn.y}, player.radius * 0.65f, kBone);
    DrawRectangleRec({spawn.x - player.radius * 0.35f, spawn.y - player.radius * 0.1f,
                      player.radius * 0.2f, player.radius * 0.2f},
                     {10, 10, 12, 255});
    DrawRectangleRec({spawn.x + player.radius * 0.15f, spawn.y - player.radius * 0.1f,
                      player.radius * 0.2f, player.radius * 0.2f},
                     {10, 10, 12, 255});
    const Vector2 aim{std::cos(facing), std::sin(facing)};
    DrawLineEx({spawn.x, spawn.y},
               {spawn.x + aim.x * player.radius * 1.7f, spawn.y + aim.y * player.radius * 1.7f},
               player.radius * 0.25f, kBone);
    EndMode2D();
    DrawRectangle(0, 0, Letterbox::kWidth, 64, {20, 22, 27, 255});
    DrawText(level.name.c_str(), 160, 20, 24, kBone);
    DrawText(player.isCrouched() ? "CROUCH" : (player.isSprinting() ? "SPRINT" : "WALK"), 440, 24,
             20, kGold);
    DrawRectangle(0, Letterbox::kHeight - 48, Letterbox::kWidth, 48, {20, 22, 27, 255});
#ifndef NDEBUG
    DrawText("WASD/arrows move | Shift sprint | C/Ctrl crouch | F3 overview | F11 fullscreen", 24,
             Letterbox::kHeight - 32, 18, kBone);
    if (overview) {
        const auto details = std::to_string(map.width()) + " x " + std::to_string(map.height()) +
                             " | Tile " + std::to_string(map.tileSize()) + " px | Seed " +
                             std::to_string(seed);
        DrawText(details.c_str(), 600, 24, 18, kTeal);
    }
#else
    (void)seed;
    DrawText("F11 fullscreen  |  ESC quit", 24, Letterbox::kHeight - 32, 18, kBone);
#endif
}
