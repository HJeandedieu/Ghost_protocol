#include "render/Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "core/Logger.h"
#include "entities/Enemy.h"
#include "entities/Guard.h"
#include "entities/Laser.h"
#include "entities/Player.h"
#include "entities/SecurityCamera.h"
#include "raylib.h"
#include "render/Letterbox.h"
#include "render/Palette.h"
#include "systems/AlarmDirector.h"
#include "systems/CombatSystem.h"
#include "systems/EnemyCombatSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/PagerSystem.h"
#include "systems/RippleSystem.h"
#include "systems/VisionSystem.h"
#include "systems/WaveSpawner.h"
#include "world/Level.h"
#include "world/Raycast.h"
#include "world/World.h"

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr int kConeArcSegments = 64;

// Original decorative geometry; collision and interactions remain map-owned.
void drawBankFurniture(int x, int y, float size, float visibility, Color wall, Color floor) {
    const Color outline = Fade(wall, visibility);
    const Color surface = Fade(floor, visibility);
    const float left = x * size;
    const float top = y * size;
    const bool office = (x == 27 || x == 31) && (y == 22 || y == 25 || y == 30 || y == 34);
    const bool console = (x == 27 || x == 30 || x == 32) && y == 14;
    const bool counter = x >= 43 && x <= 59 && y == 18;
    const bool bench = (x == 44 || x == 57) && (y == 28 || y == 36);
    const bool crate = (x == 6 || x == 10 || x == 14) && y == 34;
    if (office || console || counter || bench || crate) {
        Rectangle body{left + size * 0.12f, top + size * 0.2f, size * 0.76f, size * 0.55f};
        DrawRectangleRec(body, surface);
        DrawRectangleLinesEx(body, 2, outline);
        if (office || console) {
            DrawRectangleRec({left + size * 0.4f, top + size * 0.3f, size * 0.22f, size * 0.18f},
                             outline);
            if (office) DrawCircleV({left + size * 0.5f, top + size * 0.9f}, size * 0.09f, outline);
        }
        if (crate)
            DrawLineEx({body.x, body.y}, {body.x + body.width, body.y + body.height}, 2, outline);
    }
}

void drawClippedTriangle(Vector2 eye, Vector2 right, Vector2 left, Rectangle tile, Color color) {
    std::array<Vector2, 8> polygon{eye, right, left};
    int count = 3;
    for (int edge = 0; edge < 4 && count >= 3; ++edge) {
        std::array<Vector2, 8> clipped{};
        int output = 0;
        const bool horizontal = edge < 2;
        const float boundary = horizontal ? tile.x + (edge == 1 ? tile.width : 0)
                                          : tile.y + (edge == 3 ? tile.height : 0);
        const auto coordinate = [horizontal](Vector2 p) { return horizontal ? p.x : p.y; };
        const auto inside = [&](Vector2 p) {
            return edge % 2 == 0 ? coordinate(p) >= boundary : coordinate(p) <= boundary;
        };
        for (int i = 0; i < count; ++i) {
            const auto a = polygon[static_cast<std::size_t>(i)];
            const auto b = polygon[static_cast<std::size_t>((i + 1) % count)];
            if (inside(a) != inside(b)) {
                const float t = (boundary - coordinate(a)) / (coordinate(b) - coordinate(a));
                clipped[static_cast<std::size_t>(output++)] = {a.x + (b.x - a.x) * t,
                                                               a.y + (b.y - a.y) * t};
            }
            if (inside(b)) clipped[static_cast<std::size_t>(output++)] = b;
        }
        polygon = clipped;
        count = output;
    }
    if (count >= 3) DrawTriangleFan(polygon.data(), count, color);
}
}  // namespace

void Renderer::prepareLevel(const Level& level) {
    const auto corners = static_cast<std::size_t>(level.map.width() + 1) * (level.map.height() + 1);
    const auto capacity = corners * 3 + kConeArcSegments + 1;
    coneAngles_.reserve(capacity);
    coneDistances_.reserve(capacity);
    coneDirections_.reserve(capacity);
    coneLightRegions_.reserve(static_cast<std::size_t>(level.map.width()) * level.map.height());
}

void Renderer::drawGuardCone(const Guard& guard, Vec2 position, const TileMap& map, bool crouched,
                             float visibility) {
    const auto& config = guard.visionConfig();
    const VisionSystem vision(config);
    const float range = std::max({vision.rangeFor(LightLevel::Lit, crouched),
                                  vision.rangeFor(LightLevel::Dim, crouched),
                                  vision.rangeFor(LightLevel::Dark, crouched)});
    if (range <= 0 || config.coneDeg <= 0) return;
    const float size = static_cast<float>(map.tileSize());
    const float half = config.coneDeg * kPi / 360;
    const int x0 = std::max(0, static_cast<int>(std::floor((position.x - range) / size)));
    const int y0 = std::max(0, static_cast<int>(std::floor((position.y - range) / size)));
    const int x1 =
        std::min(map.width() - 1, static_cast<int>(std::floor((position.x + range) / size)));
    const int y1 =
        std::min(map.height() - 1, static_cast<int>(std::floor((position.y + range) / size)));
    coneAngles_.clear();
    coneDistances_.clear();
    coneDirections_.clear();
    for (int i = 0; i <= kConeArcSegments; ++i)
        coneAngles_.push_back(-half + 2 * half * i / kConeArcSegments);
    // Rays immediately to either side of wall corners preserve sharp occlusion edges.
    for (int y = y0; y <= y1 + 1; ++y)
        for (int x = x0; x <= x1 + 1; ++x) {
            if (!map.blocksSight(x, y) && !map.blocksSight(x - 1, y) &&
                !map.blocksSight(x, y - 1) && !map.blocksSight(x - 1, y - 1))
                continue;
            const float dx = x * size - position.x, dy = y * size - position.y;
            if (std::hypot(dx, dy) > range) continue;
            const float angle = std::remainder(std::atan2(dy, dx) - guard.facing(), 2 * kPi);
            for (const float offset : {-0.00001f, 0.0f, 0.00001f})
                if (angle + offset > -half && angle + offset < half)
                    coneAngles_.push_back(angle + offset);
        }
    std::sort(coneAngles_.begin(), coneAngles_.end());
    coneAngles_.erase(std::unique(coneAngles_.begin(), coneAngles_.end()), coneAngles_.end());
    for (const float angle : coneAngles_) {
        const Vec2 direction{std::cos(guard.facing() + angle), std::sin(guard.facing() + angle)};
        coneDirections_.push_back(direction);
        coneDistances_.push_back(Raycast::sightDistance(
            position, {position.x + direction.x * range, position.y + direction.y * range}, map));
    }
    const bool alerted = guard.state() == GuardState::Alerted;
    const Color color = Fade(alerted ? Color{255, 59, 92, 255} : Color{233, 228, 208, 255},
                             (alerted ? 0.25f : 0.18f) * visibility);
    coneLightRegions_.clear();
    // Merge equal-light tiles into rectangles. Occlusion is already in the ray fan,
    // so uniform rooms need one range clip rather than hundreds of tile clips.
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1;) {
            const int start = x;
            const float tileRange = vision.rangeFor(map.light(x, y), crouched);
            while (x <= x1 && vision.rangeFor(map.light(x, y), crouched) == tileRange) ++x;
            const float width = (x - start) * size;
            bool merged = false;
            for (auto& region : coneLightRegions_) {
                if (region.range == tileRange && region.bounds.x == start * size &&
                    region.bounds.width == width &&
                    region.bounds.y + region.bounds.height == y * size) {
                    region.bounds.height += size;
                    merged = true;
                    break;
                }
            }
            if (!merged)
                coneLightRegions_.push_back({{start * size, y * size, width, size}, tileRange});
        }
    }
    for (const auto& region : coneLightRegions_) {
        const float tileRange = region.range;
        const Rectangle tile = region.bounds;
        const float dx = position.x - std::clamp(position.x, tile.x, tile.x + tile.width);
        const float dy = position.y - std::clamp(position.y, tile.y, tile.y + tile.height);
        if (dx * dx + dy * dy > tileRange * tileRange) continue;
        const float centerX = tile.x + tile.width * 0.5f - position.x;
        const float centerY = tile.y + tile.height * 0.5f - position.y;
        const float distance = std::hypot(centerX, centerY);
        const float bound = std::hypot(tile.width, tile.height) * 0.5f;
        const bool nearEye = distance <= bound;
        const float center =
            nearEye ? 0 : std::remainder(std::atan2(centerY, centerX) - guard.facing(), 2 * kPi);
        const float span = nearEye ? kPi : std::asin(bound / distance);
        // A tile's circumscribed circle bounds the fan sectors that can touch it.
        for (int wrap = -1; wrap <= 1; ++wrap) {
            if (nearEye && wrap != 0) continue;
            const float low = center - span + wrap * 2 * kPi;
            const float high = center + span + wrap * 2 * kPi;
            if (high < -half || low > half) continue;
            const auto first = std::max<std::size_t>(
                1, static_cast<std::size_t>(
                       std::lower_bound(coneAngles_.begin(), coneAngles_.end(), low) -
                       coneAngles_.begin()));
            const auto last =
                std::min(coneAngles_.size() - 1,
                         static_cast<std::size_t>(
                             std::upper_bound(coneAngles_.begin(), coneAngles_.end(), high) -
                             coneAngles_.begin()));
            for (std::size_t i = first; i <= last; ++i) {
                const float leftLength = std::min(tileRange, coneDistances_[i - 1]);
                const float rightLength = std::min(tileRange, coneDistances_[i]);
                const Vector2 left{position.x + coneDirections_[i - 1].x * leftLength,
                                   position.y + coneDirections_[i - 1].y * leftLength};
                const Vector2 right{position.x + coneDirections_[i].x * rightLength,
                                    position.y + coneDirections_[i].y * rightLength};
                if (std::max({position.x, left.x, right.x}) < tile.x ||
                    std::min({position.x, left.x, right.x}) > tile.x + tile.width ||
                    std::max({position.y, left.y, right.y}) < tile.y ||
                    std::min({position.y, left.y, right.y}) > tile.y + tile.height)
                    continue;
                drawClippedTriangle({position.x, position.y}, right, left, tile, color);
            }
        }
    }
}

Renderer::Renderer(Logger& logger, const RenderConfig& config)
    : surface_(LoadRenderTexture(Letterbox::kWidth, Letterbox::kHeight)),
      world_(LoadRenderTexture(Letterbox::kWidth, Letterbox::kHeight)),
      config_(config) {
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
                         std::uint32_t seed, const std::vector<Guard>& guards,
                         const std::vector<SecurityCamera>& cameras,
                         const std::vector<Laser>& lasers, bool securityLooped,
                         const CombatSystem* combat,
                         const std::vector<std::unique_ptr<RecoveryPickup>>& pickups,
                         bool pickupsLit, const std::vector<std::unique_ptr<Enemy>>& enemies,
                         const EnemyCombatSystem* enemyCombat) {
    constexpr Color kBone = {233, 228, 208, 255};
    constexpr Color kGold = {242, 183, 5, 255};
    constexpr Color kAlarm = {255, 59, 92, 255};
    const Color wallColor = pickupsLit ? Palette::Alarm : Palette::Teal;
    const Color floorColor = pickupsLit ? Palette::DarkAlarm : Palette::DeepTeal;
    const auto& map = level.map;
    if (!guards.empty()) prepareLevel(level);
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
            const float reveal =
                overview || pickupsLit ? 1.0f : ripple.visibility(x, y, player.pos, map);
            if (tile == TileType::Wall) {
                if (reveal > 0)
                    DrawRectangleRec({x * size, y * size, size, size}, Fade(wallColor, reveal));
                const Color edge = Fade(wallColor, std::max(reveal, config_.ambientWallAlpha));
                if (map.isPassable(x - 1, y)) DrawRectangleRec({x * size, y * size, 2, size}, edge);
                if (map.isPassable(x + 1, y))
                    DrawRectangleRec({(x + 1) * size - 2, y * size, 2, size}, edge);
                if (map.isPassable(x, y - 1)) DrawRectangleRec({x * size, y * size, size, 2}, edge);
                if (map.isPassable(x, y + 1))
                    DrawRectangleRec({x * size, (y + 1) * size - 2, size, 2}, edge);
            } else {
                DrawRectangleRec({x * size, y * size, size, size},
                                 Fade(floorColor, std::max(reveal, config_.ambientFloorAlpha)));
            }
            if (level.name == "Gotham Central Bank" && tile == TileType::Floor)
                drawBankFurniture(x, y, size, std::max(reveal, config_.ambientFloorAlpha),
                                  wallColor, floorColor);
            if (reveal > 0 && tile != TileType::Wall && tile != TileType::Floor &&
                !map.isOpen(x, y) && tile != TileType::PlayerSpawn) {
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
            if (!overview && !pickupsLit) {
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
            guard.deathOpacity() *
            (overview || pickupsLit
                 ? 1.0f
                 : std::max(guard.reveal, ripple.visibility(tx, ty, player.pos, map)));
        if (visible <= 0) continue;
        if (guard.dead() || guard.state() == GuardState::Unconscious) {
            DrawEllipse(static_cast<int>(position.x), static_cast<int>(position.y),
                        guard.radius * 1.3f, guard.radius * 0.55f, Fade(kBone, visible));
            if (overview)
                DrawText(guard.id.c_str(), static_cast<int>(position.x + guard.radius),
                         static_cast<int>(position.y), 24, kBone);
            continue;
        }
        if (!pickupsLit) drawGuardCone(guard, position, map, player.isCrouched(), visible);
        if (pickupsLit)
            DrawRing({position.x, position.y}, guard.radius, guard.radius + 2, 0, 360, 64,
                     Fade(kBone, visible));
        DrawCircleV({position.x, position.y}, guard.radius, Fade(kBone, visible));
        const Vector2 direction{std::cos(guard.facing()), std::sin(guard.facing())};
        DrawLineEx({position.x, position.y},
                   {position.x + direction.x * guard.radius * 1.7f,
                    position.y + direction.y * guard.radius * 1.7f},
                   guard.radius * 0.25f, Fade(kGold, visible));
        if (overview)
            DrawText(guard.id.c_str(), static_cast<int>(position.x + guard.radius),
                     static_cast<int>(position.y), 24, kBone);
        if (guard.detection() > 0) {
            const Vector2 meter{position.x, position.y - guard.radius - 12};
            DrawCircleV(meter, 8, Fade({20, 22, 27, 255}, visible));
            DrawCircleSector(meter, 8, -90, -90 + 360 * guard.detection() / 100, 32,
                             Fade(kAlarm, visible));
            DrawCircleLinesV(meter, 8, Fade(kBone, visible));
            if (guard.state() == GuardState::Alerted)
                DrawText(TextFormat("%.1fs", guard.callInRemaining()),
                         static_cast<int>(meter.x + 12), static_cast<int>(meter.y - 8), 16,
                         Fade(kAlarm, visible));
        }
    }
    for (const auto& enemy : enemies) {
        const Vec2 p{enemy->prevPos.x + (enemy->pos.x - enemy->prevPos.x) * alpha,
                     enemy->prevPos.y + (enemy->pos.y - enemy->prevPos.y) * alpha};
        const float visible =
            enemy->deathOpacity() *
            (overview || pickupsLit
                 ? 1.f
                 : std::max(enemy->reveal, ripple.visibility(static_cast<int>(enemy->pos.x / size),
                                                             static_cast<int>(enemy->pos.y / size),
                                                             player.pos, map)));
        if (visible <= 0) continue;
        if (enemy->dead()) {
            DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), enemy->radius * 1.3f,
                        enemy->radius * 0.55f, Fade({27, 42, 74, 255}, visible));
            continue;
        }
        const bool heavy = enemy->spec().id == "heavy";
        const Color body = heavy ? Color{16, 21, 31, 255} : Color{27, 42, 74, 255};
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), enemy->radius * 1.2f,
                    enemy->radius * 0.72f, Fade(body, visible));
        DrawCircleV({p.x, p.y}, enemy->radius, Fade(body, visible));
        if (pickupsLit)
            DrawRing({p.x, p.y}, enemy->radius, enemy->radius + 2, 0, 360, 64,
                     Fade(kBone, visible));
        DrawRectangle(static_cast<int>(p.x - 6), static_cast<int>(p.y - 3), 12, 6,
                      Fade(heavy ? kGold : kBone, visible));
        DrawLineEx({p.x, p.y},
                   {p.x + std::cos(enemy->facing()) * enemy->radius * 1.7f,
                    p.y + std::sin(enemy->facing()) * enemy->radius * 1.7f},
                   4, Fade(kGold, visible));
        if (enemy->spec().id == "shield_cop") {
            const float degrees = enemy->facing() * 180 / 3.14159265358979323846f;
            const float half = enemy->spec().shieldArcDeg * 0.5f;
            DrawRing({p.x, p.y}, enemy->radius * 1.12f, enemy->radius * 1.35f, degrees - half,
                     degrees + half, 24, Fade(kBone, visible));
        }
    }
    if (enemyCombat)
        for (const auto& shot : enemyCombat->shots()) {
            DrawLineEx({shot.from.x, shot.from.y}, {shot.to.x, shot.to.y}, 2,
                       Fade(kGold, 1 - shot.age / 0.08f));
            if (shot.age < 0.05f)
                DrawCircleV({shot.from.x, shot.from.y}, 5, Fade(kGold, 1 - shot.age / 0.05f));
        }
    for (const auto& pickup : pickups) {
        const int tx = static_cast<int>(pickup->pos.x / map.tileSize());
        const int ty = static_cast<int>(pickup->pos.y / map.tileSize());
        const float visible =
            overview || pickupsLit
                ? 1.0f
                : std::max(pickup->reveal, ripple.visibility(tx, ty, player.pos, map));
        if (visible <= 0) continue;
        const auto bone = Fade(kBone, visible);
        const auto gold = Fade(kGold, visible);
        const float x = pickup->pos.x, y = pickup->pos.y;
        DrawRectangleRounded({x - 12, y - 10, 24, 20}, 0.2f, 4, Fade({20, 22, 27, 255}, visible));
        DrawRectangleLinesEx({x - 12, y - 10, 24, 20}, 2, bone);
        if (pickup->type == PickupType::Medkit) {
            DrawRectangle(static_cast<int>(x - 2), static_cast<int>(y - 6), 4, 12, gold);
            DrawRectangle(static_cast<int>(x - 6), static_cast<int>(y - 2), 12, 4, gold);
        } else {
            DrawPoly({x, y}, 5, 7, -90, gold);
        }
    }
    for (const auto& laser : lasers) {
        const float visible = overview || pickupsLit ? 1.0f : laser.reveal;
        if (visible <= 0) continue;
        const auto color = Fade(securityLooped ? kBone : kAlarm, visible);
        DrawLineEx({laser.pos.x, laser.pos.y}, {laser.end().x, laser.end().y}, 3, color);
        for (const auto point : {laser.pos, laser.end()})
            DrawRectangle(static_cast<int>(point.x - 4), static_cast<int>(point.y - 4), 8, 8,
                          color);
    }
    constexpr float kDegreesToRadians = 3.14159265358979323846f / 180;
    for (const auto& camera : cameras) {
        const float visible = overview || pickupsLit ? 1.0f : camera.reveal;
        if (visible <= 0) continue;
        const Vector2 eye{camera.pos.x, camera.pos.y};
        if (!securityLooped) {
            constexpr int kSegments = 64;
            for (int i = 0; i < kSegments; ++i) {
                const float a = (camera.facing() - camera.coneDegrees() * 0.5f +
                                 camera.coneDegrees() * i / kSegments) *
                                kDegreesToRadians;
                const float b = a + camera.coneDegrees() / kSegments * kDegreesToRadians;
                const Vec2 farA{eye.x + std::cos(a) * camera.range(),
                                eye.y + std::sin(a) * camera.range()};
                const Vec2 farB{eye.x + std::cos(b) * camera.range(),
                                eye.y + std::sin(b) * camera.range()};
                const float rangeA = Raycast::sightDistance(camera.pos, farA, map);
                const float rangeB = Raycast::sightDistance(camera.pos, farB, map);
                DrawTriangle(eye, {eye.x + std::cos(b) * rangeB, eye.y + std::sin(b) * rangeB},
                             {eye.x + std::cos(a) * rangeA, eye.y + std::sin(a) * rangeA},
                             Fade(kAlarm, visible * 0.25f));
            }
        }
        const float angle = camera.facing() * kDegreesToRadians;
        const Vector2 forward{std::cos(angle), std::sin(angle)};
        DrawTriangle({eye.x + forward.x * 12, eye.y + forward.y * 12},
                     {eye.x - forward.x * 6 + forward.y * 8, eye.y - forward.y * 6 - forward.x * 8},
                       {eye.x - forward.x * 6 - forward.y * 8, eye.y - forward.y * 6 + forward.x * 8},
                     Fade(securityLooped ? kBone : kAlarm, visible));
        if (camera.detection() > 0) {
            const Vector2 meter{eye.x, eye.y - 20};
            DrawCircleSector(meter, 8, -90, -90 + 360 * camera.detection() / 100, 32,
                             Fade(kAlarm, visible));
            DrawCircleLinesV(meter, 8, Fade(kBone, visible));
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
    if (pickupsLit)
        DrawRing({spawn.x, spawn.y}, player.radius, player.radius + 2, 0, 360, 64, kBone);
    if (!overview && !pickupsLit)
        DrawRing({spawn.x, spawn.y}, player.radius + 4, player.radius + 6, -90,
                 -90 + 360 * ripple.cooldownFraction(), 64, kBone);
    if (combat && combat->shotAge() < 0.08f) {
        const float opacity = 1.0f - combat->shotAge() / 0.08f;
        for (const auto& pellet : combat->lastShot().pellets) {
            const float distance =
                std::hypot(pellet.to.x - pellet.from.x, pellet.to.y - pellet.from.y);
            if (distance <= player.radius * 1.7f) continue;
            const float fraction = player.radius * 1.7f / distance;
            const Vector2 muzzle{pellet.from.x + (pellet.to.x - pellet.from.x) * fraction,
                                 pellet.from.y + (pellet.to.y - pellet.from.y) * fraction};
            DrawLineEx(muzzle, {pellet.to.x, pellet.to.y}, 2, Fade(kGold, opacity));
        }
        if (!reduceEffects_ && combat->shotAge() < 0.05f && !combat->lastShot().pellets.empty()) {
            const auto& shot = combat->lastShot().pellets.front();
            const float angle = shot.dirDeg * (kPi / 180.0f);
            const Vector2 forward{std::cos(angle), std::sin(angle)};
            const Vector2 muzzle{shot.from.x + forward.x * player.radius * 1.7f,
                                 shot.from.y + forward.y * player.radius * 1.7f};
            DrawTriangle({muzzle.x + forward.x * 14, muzzle.y + forward.y * 14},
                         {muzzle.x + forward.y * 6, muzzle.y - forward.x * 6},
                         {muzzle.x - forward.y * 6, muzzle.y + forward.x * 6}, kGold);
        }
    }
    EndMode2D();
    compose(true);
    if (!reduceEffects_ && healthHud_.flashFraction() > 0)
        DrawRectangle(0, 64, Letterbox::kWidth, Letterbox::kHeight - 112,
                      Fade({255, 59, 92, 255}, healthHud_.flashFraction() * 0.12f));

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
        DrawText(details.c_str(), 600, 24, 18, Palette::Teal);
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
    if (!world.alarmLoud) {
        const int filled =
            maximumNoise > 0
                ? std::clamp(static_cast<int>(std::ceil(noiseRadius / maximumNoise * 6)), 0, 6)
                : 0;
        DrawText("NOISE", 40, 136, 14, kBone);
        for (int i = 0; i < 6; ++i)
            DrawRectangle(104 + i * 24, 136, 16, 12, i < filled ? kTeal : Color{40, 44, 50, 255});
    }
    if (world.securityLoopRemaining > 0)
        DrawText(TextFormat("SECURITY LOOP %.0fs", world.securityLoopRemaining), 288, 136, 14,
                 kTeal);
    if (const auto* target = interaction.target()) {
        DrawRectangle(312, Letterbox::kHeight - 112, 704, 56, kSlate);
        DrawText(target->prompt.c_str(), 336, Letterbox::kHeight - 96, 20, kBone);
        if (interaction.progress() > 0)
            DrawRing({960, static_cast<float>(Letterbox::kHeight - 84)}, 14, 18, -90,
                     -90 + 360 * interaction.progress(), 64, kBone);
    }
}

void Renderer::drawPickupHud(const RecoveryPickup* pickup) const {
    if (!pickup) return;
    DrawRectangle(312, Letterbox::kHeight - 112, 704, 56, {20, 22, 27, 255});
    DrawText(pickup->type == PickupType::Medkit ? "E: collect medkit" : "E: collect armor plate",
             336, Letterbox::kHeight - 96, 20, {233, 228, 208, 255});
}

void Renderer::drawStealthHud(const AlarmDirector& alarm, const PagerSystem& pagers,
                              const std::vector<Guard>& guards) const {
    constexpr Color kAlarm = {255, 59, 92, 255};
    constexpr Color kBone = {233, 228, 208, 255};
    if (alarm.state() == AlarmState::CallIn)
        DrawText(TextFormat("SPOTTED %.1fs", alarm.callInRemaining()), 560, 96, 24, kAlarm);
    else if (alarm.state() == AlarmState::Loud)
        DrawText("ALARM", 560, 96, 24, kAlarm);
    int y = 136;
    for (const auto& guard : guards)
        if (pagers.state(guard.id) == PagerState::Ringing) {
            DrawText(TextFormat("PAGER %s %.1fs", guard.id.c_str(), pagers.remaining(guard.id)),
                     560, y, 18, kBone);
            y += 24;
        }
}

void Renderer::drawWaveHud(const WaveSpawner& waves) const {
    constexpr Color kBone{233, 228, 208, 255};
    constexpr Color kGold{242, 183, 5, 255};
    DrawRectangleRounded({1010, 80, 246, 88}, 0.1f, 4, {20, 22, 27, 255});
    DrawRectangleLinesEx({1010, 80, 246, 88}, 1, Fade(kBone, 0.65f));
    DrawText(TextFormat("ASSAULT  %d", waves.waveIndex()), 1026, 96, 22, kGold);
    DrawText(TextFormat("NEXT WAVE  %.0fs", std::ceil(waves.nextWaveRemaining())), 1026, 130, 16,
             kBone);
}
void Renderer::drawWeaponHud(const CombatSystem& combat) const {
    const auto& weapon = combat.activeWeapon();
    constexpr Color kBone{233, 228, 208, 255};
    constexpr Color kGold{242, 183, 5, 255};
    DrawRectangle(24, 510, 272, 146, {20, 22, 27, 255});
    DrawRectangleLinesEx({24, 510, 272, 146}, 1, Fade(kBone, 0.6f));
    const std::string name = std::to_string(combat.activeSlot() + 1) + "  " + weapon.spec().name;
    DrawText(name.c_str(), 40, 522, 22, kBone);
    const std::string ammunition =
        std::to_string(weapon.ammunition()) + " / " + std::to_string(weapon.reserve());
    DrawText(ammunition.c_str(), 40, 554, 26, kGold);
    if (weapon.reloadRemaining() > 0) {
        DrawText("RELOADING", 158, 560, 16, kBone);
        const float progress = 1 - weapon.reloadRemaining() / weapon.spec().reload;
        DrawRectangle(40, 584, static_cast<int>(232 * progress), 3, kGold);
    }
    if (!healthHud_.initialized()) return;
    const auto meter = [](float shown, float maximum, float y, Color color, const char* label) {
        DrawText(label, 40, static_cast<int>(y) - 14, 10, {233, 228, 208, 255});
        DrawRectangleRounded({40, y, 240, 12}, 1, 8, {40, 44, 50, 255});
        const float fraction = maximum > 0 ? std::clamp(shown / maximum, 0.0f, 1.0f) : 0;
        if (fraction > 0) DrawRectangleRounded({40, y, 240 * fraction, 12}, 1, 8, color);
    };
    meter(healthHud_.displayedHp(), healthHud_.maximumHp(), 606, {255, 59, 92, 255}, "HEALTH");
    meter(healthHud_.displayedArmor(), healthHud_.maximumArmor(), 636, kBone, "ARMOR");
}
