#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/Logger.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "raylib.h"
#include "render/Letterbox.h"
#include "systems/InteractionSystem.h"
#include "systems/RippleSystem.h"
#include "world/Level.h"
#include "world/Raycast.h"
#include "world/World.h"

Renderer::Renderer(Logger& logger)
    : surface_(LoadRenderTexture(Letterbox::kWidth, Letterbox::kHeight)),
      world_(LoadRenderTexture(Letterbox::kWidth, Letterbox::kHeight)) {
#ifdef __EMSCRIPTEN__
    const char* path = "assets/shaders/glsl100/post.fs";
#else
    const char* path = "assets/shaders/glsl330/post.fs";
#endif
    if (FileExists(path)) {
        post_ = LoadShader(nullptr, path);
        timeLocation_ = GetShaderLocation(post_, "elapsedTime");
    }
    if (timeLocation_ < 0)
        logger.log(LogLevel::Error, "Post shader unavailable; using plain rendering");
}
Renderer::~Renderer() {
    if (timeLocation_ >= 0) UnloadShader(post_);
    UnloadRenderTexture(world_);
    UnloadRenderTexture(surface_);
}

void Renderer::beginFrame() {
    constexpr Color kInk = {10, 10, 12, 255};
    composed_ = false;
    BeginTextureMode(world_);
    ClearBackground(kInk);
}

void Renderer::compose(bool effects) {
    EndTextureMode();
    BeginTextureMode(surface_);
    ClearBackground({10, 10, 12, 255});
    const bool useShader = effects && !reduceEffects_ && timeLocation_ >= 0;
    if (useShader) {
        const float elapsed = static_cast<float>(GetTime());
        SetShaderValue(post_, timeLocation_, &elapsed, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(post_);
    }
    DrawTextureRec(
        world_.texture,
        {0, 0, static_cast<float>(Letterbox::kWidth), -static_cast<float>(Letterbox::kHeight)},
        {0, 0}, WHITE);
    if (useShader) EndShaderMode();
    composed_ = true;
}

void Renderer::present() {
    if (!composed_) compose(false);
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

void Renderer::drawLevel(const Level& level, const Player& player, const RippleSystem& ripple,
                         Vec2 cameraTarget, float facing, float alpha, bool overview,
                         std::uint32_t seed, const std::vector<Guard>& guards) {
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
            const auto topLeft = GetWorldToScreen2D({x * size, y * size}, camera);
            const float screenSize = size * camera.zoom;
            if (topLeft.x + screenSize < 0 || topLeft.y + screenSize < 0 ||
                topLeft.x >= Letterbox::kWidth || topLeft.y >= Letterbox::kHeight)
                continue;
            const float reveal = overview ? 1.0f : ripple.visibility(x, y, player.pos, map);
            if (reveal <= 0) continue;
            DrawRectangleRec({x * size, y * size, size, size},
                             Fade(tile == TileType::Wall ? kTeal : kDeepTeal, reveal));
            if (tile != TileType::Wall && tile != TileType::Floor && !map.isOpen(x, y) &&
                tile != TileType::PlayerSpawn) {
                const auto center = map.tileCenter({x, y});
                const auto color = map.isPassable(x, y) ? kGold : kBone;
                DrawRectangleRec(
                    {center.x - size * 0.2f, center.y - size * 0.2f, size * 0.4f, size * 0.4f},
                    Fade(color, reveal));
                if (overview) {
                    const char symbol[] = {static_cast<char>(tile), '\0'};
                    DrawText(symbol, static_cast<int>(x * size + size * 0.25f),
                             static_cast<int>(y * size + size * 0.25f),
                             static_cast<int>(size * 0.5f), {10, 10, 12, 255});
                }
            }
            if (!overview) {
                const auto center = map.tileCenter({x, y});
                const bool halo = std::hypot(center.x - player.pos.x, center.y - player.pos.y) <=
                                      ripple.haloRadius() + size &&
                                  Raycast::hasLineOfSight(player.pos, center, map, true);
                const bool wave = ripple.waveActive() &&
                                  Raycast::hasLineOfSight(ripple.origin(), center, map, true);
                if (halo || wave) {
                    BeginScissorMode(static_cast<int>(std::floor(topLeft.x)),
                                     static_cast<int>(std::floor(topLeft.y)),
                                     static_cast<int>(std::ceil(screenSize)),
                                     static_cast<int>(std::ceil(screenSize)));
                    BeginBlendMode(BLEND_ADDITIVE);
                    if (halo)
                        DrawCircleGradient(static_cast<int>(spawn.x), static_cast<int>(spawn.y),
                                           ripple.haloRadius(), Fade(kBone, 0.12f), Fade(kBone, 0));
                    if (wave) {
                        const auto origin = ripple.origin();
                        const float radius = ripple.waveRadius();
                        DrawCircleV({origin.x, origin.y}, radius, Fade(kBone, 0.08f));
                        if (radius > 0)
                            DrawRing({origin.x, origin.y}, std::max(0.0f, radius - 3.0f), radius, 0,
                                     360, 128,
                                     Fade(kBone, 0.9f * (1.0f - radius / ripple.maxRadius())));
                    }
                    EndBlendMode();
                    EndScissorMode();
                }
            }
        }
    }
    for (const auto& guard : guards) {
        const auto position = guard.interpolatedPosition(alpha);
        const int tx = static_cast<int>(guard.pos.x / size);
        const int ty = static_cast<int>(guard.pos.y / size);
        const float visible =
            overview ? 1.0f : std::max(guard.reveal, ripple.visibility(tx, ty, player.pos, map));
        if (visible <= 0) continue;
        DrawCircleV({position.x, position.y}, guard.radius, Fade(kBone, visible));
        const Vector2 direction{std::cos(guard.facing()), std::sin(guard.facing())};
        DrawLineEx({position.x, position.y},
                   {position.x + direction.x * guard.radius * 1.7f,
                    position.y + direction.y * guard.radius * 1.7f},
                   guard.radius * 0.25f, Fade(kGold, visible));
        if (overview)
            DrawText(guard.id.c_str(), static_cast<int>(position.x + guard.radius),
                     static_cast<int>(position.y), 24, kBone);
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
    if (!overview)
        DrawRing({spawn.x, spawn.y}, player.radius + 4, player.radius + 6, -90,
                 -90 + 360 * ripple.cooldownFraction(), 64, kBone);
    EndMode2D();
    compose(true);
    DrawRectangle(0, 0, Letterbox::kWidth, 64, {20, 22, 27, 255});
    DrawText(level.name.c_str(), 160, 20, 24, kBone);
    DrawText(player.isCrouched() ? "CROUCH" : (player.isSprinting() ? "SPRINT" : "WALK"), 440, 24,
             20, kGold);
    DrawRectangle(0, Letterbox::kHeight - 48, Letterbox::kWidth, 48, {20, 22, 27, 255});
#ifndef NDEBUG
    DrawText("WASD/arrows | Shift sprint | C/Ctrl crouch | Space ping | E interact | F3 overview",
             24, Letterbox::kHeight - 32, 18, kBone);
    if (overview) {
        const auto details = std::to_string(map.width()) + " x " + std::to_string(map.height()) +
                             " | Tile " + std::to_string(map.tileSize()) + " px | Seed " +
                             std::to_string(seed);
        DrawText(details.c_str(), 600, 24, 18, kTeal);
    }
#else
    (void)seed;
    DrawText(
        "WASD/arrows | Shift sprint | C/Ctrl crouch | Space ping | E interact | F11 fullscreen", 24,
        Letterbox::kHeight - 32, 18, kBone);
#endif
}

void Renderer::drawInteractionHud(const World& world, const InteractionSystem& interaction,
                                  float noiseRadius, float maximumNoise) const {
    constexpr Color kBone = {233, 228, 208, 255};
    constexpr Color kTeal = {63, 143, 140, 255};
    constexpr Color kSlate = {20, 22, 27, 255};
    const char* objective =
        world.powerOn ? "Gate open: reach the vault corridor"
                      : (world.player.hasKeycard() ? "Find the breaker and restore gate power"
                                                   : "Enter the bank and find the red keycard");
    DrawRectangle(24, 80, 520, 88, kSlate);
    DrawText(objective, 40, 96, 18, kBone);
    const int filled =
        maximumNoise > 0
            ? std::clamp(static_cast<int>(std::ceil(noiseRadius / maximumNoise * 6)), 0, 6)
            : 0;
    DrawText("NOISE", 40, 136, 14, kBone);
    for (int i = 0; i < 6; ++i)
        DrawRectangle(104 + i * 24, 136, 16, 12, i < filled ? kTeal : Color{40, 44, 50, 255});
    if (world.securityLoopRemaining > 0)
        DrawText(TextFormat("SECURITY LOOP %.0fs", world.securityLoopRemaining), 288, 136, 14,
                 kTeal);
    if (const auto* target = interaction.target()) {
        DrawRectangle(264, Letterbox::kHeight - 112, 752, 56, kSlate);
        DrawText(target->prompt.c_str(), 288, Letterbox::kHeight - 96, 20, kBone);
        if (interaction.progress() > 0)
            DrawRing({960, static_cast<float>(Letterbox::kHeight - 84)}, 14, 18, -90,
                     -90 + 360 * interaction.progress(), 64, kBone);
    }
}
