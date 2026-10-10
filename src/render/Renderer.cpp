#include "render/Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core/FirstPersonView.h"
#include "core/Logger.h"
#include "core/ViewFootprint.h"
#include "entities/Enemy.h"
#include "entities/Guard.h"
#include "entities/Laser.h"
#include "entities/Player.h"
#include "entities/SecurityCamera.h"
#include "raylib.h"
#include "render/BankScene.h"
#include "render/TacticalArt.h"
#include "rlgl.h"
#ifdef __EMSCRIPTEN__
#include <GLES2/gl2.h>
#else
#include "external/glad.h"
#endif
#include "render/AlarmSequence.h"
#include "render/Letterbox.h"
#include "render/Palette.h"
#include "systems/AlarmDirector.h"
#include "systems/CombatSystem.h"
#include "systems/EnemyCombatSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/ObjectiveSystem.h"
#include "systems/PagerSystem.h"
#include "systems/RippleSystem.h"
#include "systems/ScoreSystem.h"
#include "systems/VisionSystem.h"
#include "systems/VoiceDirector.h"
#include "systems/WaveSpawner.h"
#include "ui/Widgets.h"
#include "world/Level.h"
#include "world/Raycast.h"
#include "world/World.h"

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr int kConeArcSegments = 64;

RenderTexture2D loadFilteredTarget(int width, int height) {
    auto target = LoadRenderTexture(width, height);
    if (target.texture.id != 0) SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    return target;
}

void beginLogicalTarget(RenderTexture2D target) {
    BeginTextureMode(target);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, Letterbox::kWidth, Letterbox::kHeight, 0, 0, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void drawLogicalTexture(Texture2D texture, Color tint) {
    DrawTexturePro(texture,
                   {0, 0, static_cast<float>(texture.width), -static_cast<float>(texture.height)},
                   {0, 0, 1280, 720}, {0, 0}, 0, tint);
}

// Original decorative geometry; collision and interactions remain map-owned.
void drawBankFurniture(int x, int y, float size, float visibility, Color wall, Color floor) {
    const Color edge = Fade(ColorLerp(wall, floor, .3f), visibility);
    const Color surface = Fade(ColorLerp(wall, floor, .72f), visibility);
    const Color dark = Fade(ColorLerp(floor, Palette::Ink, .7f), visibility);
    const float tileSize = size;
    const bool smallProp = !((x == 43 || x == 44 || x == 60 || x == 61) && y >= 23 && y <= 33);
    if (smallProp) size *= 1.65f;
    const float left = x * tileSize + (tileSize - size) * .5f;
    const float top = y * tileSize + (tileSize - size) * .5f;
    const auto rect = [&](float px, float py, float w, float h, Color color) {
        DrawRectangleRec({left + size * px, top + size * py, size * w, size * h}, color);
    };
    const bool desk = (x == 27 || x == 31) && (y == 22 || y == 25 || y == 30 || y == 34);
    const bool console = (x == 27 || x == 30 || x == 32) && y == 14;
    const bool counter = (x == 43 || x == 44 || x == 60 || x == 61) && y >= 23 && y <= 33;
    const bool bench = (x == 44 || x == 57) && (y == 28 || y == 36);
    const bool crate = (x == 6 || x == 10 || x == 14) && y == 34;
    if (desk || console) {
        DrawRectangleRounded({left + size * .08f, top + size * .16f, size * .86f, size * .58f},
                             .13f, 12, Fade(BLACK, visibility * .59f));
        DrawRectangleRounded({left + size * .06f, top + size * .08f, size * .88f, size * .58f},
                             .13f, 12, dark);
        DrawRectangleRounded({left + size * .09f, top + size * .09f, size * .82f, size * .52f}, .1f,
                             12, surface);
        rect(.12f, .59f, .76f, .055f, dark);
        for (int drawer = 0; drawer < 2; ++drawer) {
            rect(.14f + drawer * .57f, .42f, .13f, .13f, dark);
            rect(.17f + drawer * .57f, .44f, .07f, .015f, edge);
        }
        rect(.14f, .15f, .72f, .08f, edge);
        rect(.35f, .27f, .3f, .2f, dark);
        rect(.38f, .3f, .24f, .12f, edge);
        rect(.34f, .51f, .3f, .07f, dark);
        for (int row = 0; row < 2; ++row)
            for (int key = 0; key < 7; ++key)
                rect(.35f + key * .04f, .52f + row * .023f, .024f, .012f, edge);
        rect(.65f, .51f, .04f, .06f, dark);
        rect(.38f, .3f, .24f, .018f, Fade(Palette::Bone, visibility * .5f));
        rect(.39f, .34f, .14f, .012f, dark);
        rect(.39f, .37f, .19f, .012f, dark);
        const Vector2 chair{left + size * .5f, top + size * .83f};
        DrawCircleSector(chair, size * .14f, 0, 360, 48, dark);
        DrawRectangleRounded(
            {chair.x - size * .16f, chair.y - size * .13f, size * .32f, size * .18f}, .3f, 12,
            surface);
        rect(.32f, .88f, .36f, .045f, edge);
        rect(.31f, .76f, .025f, .14f, edge);
        rect(.67f, .76f, .025f, .14f, edge);
        rect(.73f, .50f, .11f, .09f, Fade(Palette::Bone, visibility * .45f));
        for (int line = 0; line < 3; ++line) rect(.75f, .52f + line * .018f, .07f, .008f, dark);
    }
    if (counter) {
        rect(0, 0, 1, 1, dark);
        rect(.05f, 0, .9f, 1, surface);
        rect(.25f, 0, .55f, 1, Fade(ColorLerp(wall, floor, .75f), visibility));
        rect(.25f, .96f, .55f, .025f, Fade(wall, visibility * .15f));
        if (x == 43 || x == 60) {
            rect(.06f, 0, .045f, 1, edge);
            rect(.11f, 0, .12f, 1, Fade(ColorLerp(wall, floor, .35f), visibility));
            rect(.21f, 0, .025f, 1, dark);
        }
        if (x == 44 || x == 61) rect(.86f, .05f, .08f, .9f, dark);
        if (y == 23 || y == 33) rect(0, y == 23 ? 0 : .85f, 1, .15f, edge);
        if (y % 3 == 0) {
            rect(.35f, .25f, .36f, .31f, dark);
            rect(.38f, .28f, .30f, .18f, edge);
            rect(.4f, .32f, .2f, .025f, Fade(Palette::Bone, visibility * .35f));
            rect(.47f, .56f, .13f, .09f, dark);
            rect(.32f, .68f, .42f, .12f, dark);
            for (int key = 0; key < 6; ++key) rect(.34f + key * .065f, .7f, .04f, .025f, edge);
        }
    }
    if ((x == 42 || x == 62) && y >= 23 && y <= 33 && y % 3 == 0) {
        DrawRectangleRounded({left + size * .24f, top + size * .23f, size * .52f, size * .54f},
                             .35f, 16, dark);
        DrawRectangleRounded({left + size * .29f, top + size * .29f, size * .42f, size * .37f},
                             .25f, 16, surface);
        rect(.25f, .26f, .5f, .09f, edge);
        rect(.22f, .42f, .06f, .28f, edge);
        rect(.72f, .42f, .06f, .28f, edge);
    }
    if (bench) {
        DrawRectangleRounded({left + size * .06f, top + size * .18f, size * .88f, size * .59f},
                             .16f, 16, dark);
        rect(.06f, .24f, .065f, .48f, edge);
        rect(.875f, .24f, .065f, .48f, edge);
        rect(.12f, .2f, .76f, .13f, edge);
        rect(.12f, .36f, .35f, .3f, surface);
        rect(.53f, .36f, .35f, .3f, surface);
    }
    if (crate) {
        rect(.15f, .15f, .7f, .7f, surface);
        rect(.2f, .2f, .6f, .08f, edge);
        rect(.2f, .72f, .6f, .08f, edge);
        DrawLineEx({left + size * .25f, top + size * .3f}, {left + size * .75f, top + size * .7f},
                   2, dark);
        DrawLineEx({left + size * .75f, top + size * .3f}, {left + size * .25f, top + size * .7f},
                   2, dark);
    }
    if ((x == 25 || x == 33) && (y == 18 || y == 26 || y == 36)) {
        const Vector2 center{left + size * .5f, top + size * .5f};
        DrawCircleSector({center.x + size * .05f, center.y + size * .08f}, size * .37f, 0, 360, 64,
                         Fade(BLACK, visibility * .51f));
        DrawRectangleRounded({center.x - size * .2f, center.y - size * .2f, size * .4f, size * .4f},
                             .2f, 12, dark);
        for (int i = 0; i < 10; ++i) {
            const float angle = i * 2 * kPi / 10;
            const Vector2 direction{std::cos(angle), std::sin(angle)};
            const Vector2 side{-direction.y, direction.x};
            std::array<Vector2, 18> leaf{};
            leaf[0] = center;
            for (int point = 0; point <= 16; ++point) {
                const float theta = point * 2 * kPi / 16;
                const float along = (.18f + .18f * std::cos(theta)) * size;
                const float across = .085f * size * std::sin(theta);
                leaf[point + 1] = {center.x + direction.x * along + side.x * across,
                                   center.y + direction.y * along + side.y * across};
            }
            std::reverse(leaf.begin() + 1, leaf.end());
            DrawTriangleFan(leaf.data(), static_cast<int>(leaf.size()),
                            Fade(ColorLerp(wall, floor, i % 2 == 0 ? .75f : .55f), visibility));
            DrawLineEx(center,
                       {center.x + direction.x * size * .32f, center.y + direction.y * size * .32f},
                       size * .012f, dark);
        }
        DrawCircleSector(center, size * .06f, 0, 360, 32, edge);
    }
}

void drawKeycard(Vec2 center, float size, float reveal) {
    constexpr Color kAlarm = Palette::Alarm, kBone = Palette::Bone;
    constexpr Color kGold{242, 183, 5, 255};
    const Rectangle card{center.x - size * .22f, center.y - size * .13f, size * .44f, size * .26f};
    DrawRectangleRounded({card.x + 2, card.y + 3, card.width, card.height}, .2f, 12,
                         Fade(BLACK, reveal * .5f));
    DrawRectangleRounded(card, .2f, 12, Fade(kAlarm, reveal));
    DrawRectangleRec({card.x + 2, card.y + 2, card.width - 4, size * .04f},
                     Fade(Palette::Ink, reveal));
    DrawRectangleRec({card.x + size * .06f, card.y + size * .12f, size * .07f, size * .065f},
                     Fade(kGold, reveal));
    DrawLineEx({card.x + size * .19f, card.y + size * .15f},
               {card.x + size * .35f, card.y + size * .15f}, 1, Fade(kBone, reveal));
}

bool isDoor(TileType tile) {
    return tile == TileType::Door || tile == TileType::ServiceDoor || tile == TileType::CardDoor ||
           tile == TileType::FrontDoor || tile == TileType::Gate;
}

void drawBankDoor(int x, int y, float size, float visibility, const TileMap& map, Color wall) {
    const Vector2 center{(x + .5f) * size, (y + .5f) * size};
    const bool vertical = map.blocksSight(x, y - 1) || map.blocksSight(x, y + 1);
    const Vector2 axis = vertical ? Vector2{0, 1} : Vector2{1, 0};
    const Vector2 across{-axis.y, axis.x};
    const Vector2 hinge{center.x - axis.x * size * .44f, center.y - axis.y * size * .44f};
    const Vector2 end{center.x + axis.x * size * .44f, center.y + axis.y * size * .44f};
    const Color edge = Fade(wall, visibility);
    const Color metal = Fade(ColorLerp(wall, Palette::Bone, .25f), visibility);
    for (const auto point : {hinge, end}) {
        DrawLineEx({point.x - across.x * size * .14f, point.y - across.y * size * .14f},
                   {point.x + across.x * size * .14f, point.y + across.y * size * .14f},
                   size * .08f, edge);
        DrawCircleSector(point, size * .045f, 0, 360, 32, metal);
    }
    const Vector2 tip = map.isOpen(x, y) ? Vector2{hinge.x + across.x * size * .78f,
                                                   hinge.y + across.y * size * .78f}
                                         : end;
    DrawLineEx(hinge, tip, size * .18f, Fade(Palette::Ink, visibility));
    DrawLineEx(hinge, tip, size * .12f, Fade(ColorLerp(wall, Palette::Ink, .55f), visibility));
    DrawLineEx({hinge.x - across.x * size * .045f, hinge.y - across.y * size * .045f},
               {tip.x - across.x * size * .045f, tip.y - across.y * size * .045f}, size * .022f,
               metal);
    DrawCircleSector({hinge.x + (tip.x - hinge.x) * .82f, hinge.y + (tip.y - hinge.y) * .82f},
                     size * .035f, 0, 360, 32, Fade(Palette::Bone, visibility));
}

void drawOperator(Vector2 p, float radius, float facing, Color body, float opacity, bool rim,
                  bool ghost, bool heavy = false) {
    const Vector2 forward{std::cos(facing), std::sin(facing)}, side{-forward.y, forward.x};
    const auto at = [&](float x, float y) {
        return Vector2{p.x + radius * (forward.x * x + side.x * y),
                       p.y + radius * (forward.y * x + side.y * y)};
    };
    const auto disk = [&](Vector2 center, float r, Color color) {
        DrawCircleSector(center, r, 0, 360, 64, Fade(color, opacity));
    };
    // One continuous outer silhouette: overlapping fills cover the internal rims.
    DrawCircleSector(at(-.15f, .12f), radius * 1.2f, 0, 360, 64, Fade(BLACK, opacity * .43f));
    const std::array<Vector2, 2> silhouette{at(-.28f, -.65f), at(-.28f, .65f)};
    if (rim)
        for (std::size_t i = 0; i < silhouette.size(); ++i)
            disk(silhouette[i], radius * .56f + 1.2f, Palette::Bone);
    for (std::size_t i = 0; i < silhouette.size(); ++i) disk(silhouette[i], radius * .56f, body);
    const auto torso = [&](float scale, Color color) {
        std::array<Vector2, 8> contour{
            at(-.95f * scale, -.48f * scale), at(-.65f * scale, -.8f * scale),
            at(.05f * scale, -.83f * scale),  at(.62f * scale, -.42f * scale),
            at(.68f * scale, .42f * scale),   at(.05f * scale, .83f * scale),
            at(-.65f * scale, .8f * scale),   at(-.95f * scale, .48f * scale)};
        std::reverse(contour.begin(), contour.end());
        DrawTriangleFan(contour.data(), static_cast<int>(contour.size()), Fade(color, opacity));
    };
    if (rim) torso(1.1f, Palette::Bone);
    for (const auto shoulder : silhouette) disk(shoulder, radius * .56f, body);
    torso(1.f, body);
    // Shoulder pads, articulated sleeves and the rear vest panel.
    for (float sign : {-1.f, 1.f}) {
        DrawLineEx(at(-.48f, sign * .77f), at(.27f, sign * .7f), radius * .23f,
                   Fade(ColorLerp(body, Palette::Bone, .16f), opacity));
        DrawLineEx(at(.27f, sign * .7f), at(.75f, sign * .32f), radius * .27f, Fade(body, opacity));
        disk(at(.76f, sign * .31f), radius * .16f, ghost ? Palette::Slate : Palette::Bone);
    }
    DrawLineEx(at(-.63f, -.4f), at(-.63f, .4f), radius * .3f,
               Fade(ColorLerp(body, Palette::Ink, .65f), opacity));
    DrawLineEx(at(-.63f, -.37f), at(-.63f, .37f), radius * .055f,
               Fade(heavy ? Color{242, 183, 5, 255} : Palette::Slate, opacity));
    disk(at(.04f, 0), radius * .63f, Palette::Ink);
    disk(at(.16f, 0), radius * .57f, ghost ? Palette::Bone : Palette::Slate);
    disk(at(-.03f, -.55f), radius * .17f, Palette::Ink);
    disk(at(-.03f, .55f), radius * .17f, Palette::Ink);
    if (ghost) {
        for (float eye : {-.22f, .22f}) disk(at(.35f, eye), radius * .105f, Palette::Ink);
        DrawLineEx(at(.58f, -.14f), at(.58f, .14f), radius * .09f, Fade(Palette::Ink, opacity));
        disk(at(-.08f, -.58f), radius * .065f, Palette::Alarm);
    } else {
        DrawLineEx(at(.53f, -.38f), at(.53f, .38f), radius * .2f, Fade(body, opacity));
        DrawLineEx(at(.58f, -.29f), at(.58f, .29f), radius * .055f, Fade(Palette::Bone, opacity));
    }
    // Receiver and barrel are visual geometry; combat owns shot origins and aim.
    DrawLineEx(at(.83f, -.04f), at(1.43f, -.04f), radius * .23f, Fade(Palette::Ink, opacity));
    DrawLineEx(at(1.2f, -.04f), at(1.85f, -.04f), radius * .105f, Fade(Palette::Slate, opacity));
    DrawLineEx(at(.91f, -.1f), at(1.37f, -.1f), radius * .055f,
               Fade(Color{242, 183, 5, 255}, opacity));
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
// Continuous wall-corner rays translate the tile-owned reveal into smooth
// visible floor shapes. This reads the same raycast and never writes reveal.
void sightBoundary(Vec2 origin, float radius, const TileMap& map, std::vector<float>& angles,
                   std::vector<Vector2>& boundary) {
    angles.clear();
    boundary.clear();
    if (radius <= 0) return;
    constexpr int segments = 256;
    for (int i = 0; i <= segments; ++i) angles.push_back(i * 2 * kPi / segments);
    const float size = static_cast<float>(map.tileSize());
    const int left = std::max(0, static_cast<int>((origin.x - radius) / size));
    const int top = std::max(0, static_cast<int>((origin.y - radius) / size));
    const int right = std::min(map.width(), static_cast<int>((origin.x + radius) / size) + 1);
    const int bottom = std::min(map.height(), static_cast<int>((origin.y + radius) / size) + 1);
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x) {
            if (!map.blocksSight(x, y) && !map.blocksSight(x - 1, y) &&
                !map.blocksSight(x, y - 1) && !map.blocksSight(x - 1, y - 1))
                continue;
            const float dx = x * size - origin.x, dy = y * size - origin.y;
            if (std::hypot(dx, dy) > radius) continue;
            const float angle = std::atan2(dy, dx);
            for (const float offset : {-.00001f, 0.f, .00001f}) {
                const float wrapped = std::fmod(angle + offset + 2 * kPi, 2 * kPi);
                angles.push_back(wrapped);
            }
        }
    std::sort(angles.begin(), angles.end());
    angles.erase(std::unique(angles.begin(), angles.end()), angles.end());
    for (const float angle : angles) {
        const Vec2 end{origin.x + std::cos(angle) * radius, origin.y + std::sin(angle) * radius};
        const float distance = Raycast::sightDistance(origin, end, map);
        boundary.push_back(
            {origin.x + std::cos(angle) * distance, origin.y + std::sin(angle) * distance});
    }
}

}  // namespace

void Renderer::prepareLevel(const Level& level) {
    if (tactical_) tactical_->resetMotion();
    bank_.reset();
    const auto corners = static_cast<std::size_t>(level.map.width() + 1) * (level.map.height() + 1);
    const auto capacity = corners * 3 + kConeArcSegments + 1;
    coneAngles_.reserve(capacity);
    coneDistances_.reserve(capacity);
    coneDirections_.reserve(capacity);
    coneLightRegions_.reserve(static_cast<std::size_t>(level.map.width()) * level.map.height());
    pingAngles_.reserve(capacity + 256);
    pingBoundary_.reserve(capacity + 256);
}

void Renderer::drawPerspective(const World& world, const FirstPersonView& view,
                               const RippleSystem& ripple, float alpha,
                               const AlarmSequence* sequence, const ObjectiveSystem* objectives,
                               const CombatSystem* combat, const EnemyCombatSystem* enemyCombat) {
    if (!bank_) bank_ = std::make_unique<BankScene>(logger_);
    const auto position = world.player.interpolatedPosition(alpha);
    const auto aim = view.shotRay(position, world.player.isCrouched());
    Camera3D camera{};
    camera.position = {aim.origin.x, aim.origin.y, aim.origin.z};
    const auto target = aim.at(1);
    camera.target = {target.x, target.y, target.z};
    camera.up = {0, 1, 0};
    camera.fovy = view.config().fovYDeg;
    camera.projection = CAMERA_PERSPECTIVE;
    const float phase = world.alarmLoud ? (sequence ? sequence->paletteBlend() : 1.f) : 0.f;
    alarmPulse_ = sequence && !reduceEffects_ ? sequence->vignettePulse() : 0.f;
    rlSetClipPlanes(view.config().nearClip, view.config().farClip);
    BeginMode3D(camera);
    // Cull only visual assemblies, using an intentionally generous planar bound that
    // includes weapons/limbs. Vision fans and simulation remain independent of this check.
    const auto footprint = ViewFootprint::perspective(
        aim, view.config().fovYDeg, static_cast<float>(Letterbox::kWidth) / Letterbox::kHeight,
        view.config().nearClip, view.config().farClip);
    bank_->draw(world, ripple, view.config(), config_, alpha, phase, objectives,
                combat ? combat->geometry() : ShotGeometryConfig{}, tactical_.get(), reduceEffects_,
                &footprint);
    const auto point = [](Vec3 position) { return Vector3{position.x, position.y, position.z}; };
    if (combat && combat->shotAge() < .08f) {
        const float opacity = 1 - combat->shotAge() / .08f;
        for (const auto& pellet : combat->lastShot().pellets) {
            DrawCylinderEx(point(pellet.from3D), point(pellet.to3D), .25f, .25f, 6,
                           Fade(Palette::Gold, opacity));
            if (pellet.impact != ShotImpact::None)
                DrawSphere(point(pellet.to3D), 1.2f,
                           Fade(pellet.impact == ShotImpact::Shield ? Palette::Bone : Palette::Gold,
                                opacity));
        }
    }
    if (enemyCombat)
        for (const auto& shot : enemyCombat->shots())
            DrawLine3D(point(shot.from3D), point(shot.to3D),
                       Fade(Palette::Gold, 1 - shot.age / .08f));
    EndMode3D();
    if (combat && tactical_) {
        // Clear scene depth only; the viewmodel cannot clip into nearby bank walls.
        rlDrawRenderBatchActive();
        glClear(GL_DEPTH_BUFFER_BIT);
        rlSetClipPlanes(.01, 10);
        Camera3D foreground{
            {0, 0, 0}, {0, 0, 1}, {0, 1, 0}, view.config().fovYDeg, CAMERA_PERSPECTIVE};
        BeginMode3D(foreground);
        tactical_->drawForeground(*combat, phase, reduceEffects_);
        EndMode3D();
        rlSetClipPlanes(view.config().nearClip, view.config().farClip);
    }
    compose(true);
    if (!reduceEffects_ && healthHud_.flashFraction() > 0)
        DrawRectangle(0, 64, Letterbox::kWidth, Letterbox::kHeight - 112,
                      Fade(Palette::Alarm, healthHud_.flashFraction() * .12f));
    uiAssets_.text(world.level.name.c_str(), {464, 24}, 18, Palette::Bone);
    uiAssets_.text(world.player.isCrouched()    ? "CROUCH"
                   : world.player.isSprinting() ? "SPRINT"
                                                : "WALK",
                   {464, 48}, 14, {242, 183, 5, 255}, true);
    if (objectives && objectives->thermiteRemaining() > 0) {
        DrawRectangleRounded({500, 82, 280, 44}, .1f, 8, Fade(Palette::Ink, .9f));
        uiAssets_.text(
            TextFormat("THERMITE  %d:%02d",
                       static_cast<int>(std::ceil(objectives->thermiteRemaining())) / 60,
                       static_cast<int>(std::ceil(objectives->thermiteRemaining())) % 60),
            {524, 94}, 20, Palette::Gold, true);
    }
    DrawLine(633, 360, 637, 360, Palette::Bone);
    DrawLine(643, 360, 647, 360, Palette::Bone);
    DrawLine(640, 353, 640, 357, Palette::Bone);
    DrawLine(640, 363, 640, 367, Palette::Bone);
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
    : config_(config), logger_(logger), uiAssets_(logger) {
    resizeTargets();
#ifdef __EMSCRIPTEN__
    const char* path = "assets/shaders/glsl100/post.fs";
#else
    const char* path = "assets/shaders/glsl330/post.fs";
#endif
    if (FileExists(path)) {
        post_ = LoadShader(nullptr, path);
        timeLocation_ = GetShaderLocation(post_, "elapsedTime");
        alarmPulseLocation_ = GetShaderLocation(post_, "alarmPulse");
        grainLocation_ = GetShaderLocation(post_, "grainIntensity");
        texelLocation_ = GetShaderLocation(post_, "texelSize");
        vignetteLocation_ = GetShaderLocation(post_, "vignetteStrength");
    }
    if (timeLocation_ < 0)
        logger.log(LogLevel::Error, "Post shader unavailable; using plain rendering");
}
Renderer::~Renderer() {
    if (frozen_.id) UnloadRenderTexture(frozen_);
    if (outgoing_.id) UnloadRenderTexture(outgoing_);
    if (timeLocation_ >= 0) UnloadShader(post_);
    UnloadRenderTexture(world_);
    UnloadRenderTexture(surface_);
}

void Renderer::beginLogicalScissor(int x, int y, int width, int height) const {
    const double scale = static_cast<double>(surface_.texture.width) / Letterbox::kWidth;
    const int left = static_cast<int>(std::floor(x * scale));
    const int top = static_cast<int>(std::floor(y * scale));
    BeginScissorMode(left, top, static_cast<int>(std::ceil((x + width) * scale)) - left,
                     static_cast<int>(std::ceil((y + height) * scale)) - top);
}

void Renderer::resizeTargets() {
    if (textureLimit_ == 0) glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureLimit_);
#ifdef __EMSCRIPTEN__
    const auto desired = Letterbox::renderSize(1280, 720, config_.ssaaScale, textureLimit_);
#else
    const auto desired = Letterbox::renderSize(GetRenderWidth(), GetRenderHeight(),
                                               config_.ssaaScale, textureLimit_);
#endif
    if (desired.width == requestedWidth_ && desired.height == requestedHeight_) return;
    std::array<RenderTexture2D, 4> targets{};
    const int count = static_cast<int>(targets.size());
    const auto release = [&] {
        for (auto& target : targets) {
            if (target.id) UnloadRenderTexture(target);
            target = {};
        }
    };
    const auto allocate = [&](int units) {
        release();
        if (units <= 0) return false;
        for (int i = 0; i < count; ++i) {
            targets[i] = loadFilteredTarget(units * 16, units * 9);
            if (!IsRenderTextureValid(targets[i]) || !rlFramebufferComplete(targets[i].id)) {
                release();
                return false;
            }
            BeginTextureMode(targets[i]);
            ClearBackground(BLACK);
            EndTextureMode();
        }
        return true;
    };
    int units = desired.width / 16;
    if (!allocate(units)) {
        // Find the highest allocation that the device can actually support.
        int low = 0, high = units - 1;
        while (low < high) {
            const int mid = low + (high - low + 1) / 2;
            if (allocate(mid))
                low = mid;
            else
                high = mid - 1;
        }
        units = low;
        if (!allocate(units)) {
            logger_.log(LogLevel::Warn,
                        "Render target allocation failed; retaining previous frame targets");
            if (!world_.id) throw std::runtime_error("No supported render target allocation");
            requestedWidth_ = desired.width;
            requestedHeight_ = desired.height;
            return;
        }
    }
    if (desired.reduced || units * 16 != desired.width)
        logger_.log(LogLevel::Warn, "GPU limit reduced render targets to " +
                                        std::to_string(units * 16) + "x" +
                                        std::to_string(units * 9));
    int index = 2;
    const auto preserve = [&](RenderTexture2D& old) {
        auto replacement = targets[index++];
        beginLogicalTarget(replacement);
        ClearBackground(BLACK);
        drawLogicalTexture(old.texture, WHITE);
        EndTextureMode();
        UnloadRenderTexture(old);
        old = replacement;
    };
    if (frozen_.id) preserve(frozen_);
    if (outgoing_.id) preserve(outgoing_);
    frozen_ = targets[2];
    outgoing_ = targets[3];
    if (world_.id) UnloadRenderTexture(world_);
    if (surface_.id) UnloadRenderTexture(surface_);
    world_ = targets[0];
    surface_ = targets[1];
    requestedWidth_ = desired.width;
    requestedHeight_ = desired.height;
    uiAssets_.ensureFontResolution(
        static_cast<int>(std::ceil(96.0 * surface_.texture.width / 1280)));
}

void Renderer::beginFrame() {
    if (!tactical_) tactical_ = std::make_unique<TacticalArt>(logger_);
    constexpr Color kInk = {10, 10, 12, 255};
    resizeTargets();
    composed_ = false;
    beginLogicalTarget(world_);
    ClearBackground(kInk);
}

void Renderer::compose(bool effects) {
    EndTextureMode();
    beginLogicalTarget(surface_);
    ClearBackground({10, 10, 12, 255});
    const bool useShader = effects && !reduceEffects_ && timeLocation_ >= 0;
    if (useShader) {
        const float elapsed = static_cast<float>(GetTime());
        SetShaderValue(post_, timeLocation_, &elapsed, SHADER_UNIFORM_FLOAT);
        if (grainLocation_ >= 0)
            SetShaderValue(post_, grainLocation_, &config_.grainIntensity, SHADER_UNIFORM_FLOAT);
        if (vignetteLocation_ >= 0)
            SetShaderValue(post_, vignetteLocation_, &config_.vignetteStrength,
                           SHADER_UNIFORM_FLOAT);
        if (alarmPulseLocation_ >= 0)
            SetShaderValue(post_, alarmPulseLocation_, &alarmPulse_, SHADER_UNIFORM_FLOAT);
        const float texel[2] = {1.f / world_.texture.width, 1.f / world_.texture.height};
        if (texelLocation_ >= 0) SetShaderValue(post_, texelLocation_, texel, SHADER_UNIFORM_VEC2);
        BeginShaderMode(post_);
    }
    drawLogicalTexture(world_.texture, WHITE);
    if (useShader) EndShaderMode();
    composed_ = true;
}

void Renderer::freezeFrame() {
    if (!frozen_.id) frozen_ = loadFilteredTarget(surface_.texture.width, surface_.texture.height);
    beginLogicalTarget(frozen_);
    ClearBackground(BLACK);
    drawLogicalTexture(surface_.texture, WHITE);
    EndTextureMode();
}
void Renderer::drawFrozenFrame() const {
    if (frozen_.id) drawLogicalTexture(frozen_.texture, WHITE);
}
void Renderer::startTransition(float duration, bool backwards) {
    if (!outgoing_.id)
        outgoing_ = loadFilteredTarget(surface_.texture.width, surface_.texture.height);
    beginLogicalTarget(outgoing_);
    ClearBackground(BLACK);
    drawLogicalTexture(surface_.texture, WHITE);
    EndTextureMode();
    transitionDuration_ = duration;
    transitionElapsed_ = 0;
    transitionBackwards_ = backwards;
}

void Renderer::present() {
    if (!composed_) compose(false);
    if (transitionElapsed_ < transitionDuration_ && outgoing_.id) {
        const float p = transitionProgress(transitionElapsed_, transitionDuration_);
        if (reduceEffects_) {
            drawLogicalTexture(outgoing_.texture, Fade(WHITE, 1 - p));
        } else {
            // Three staggered directional panels retain the outgoing composition.
            for (int row = 0; row < 3; ++row) {
                const float local = std::clamp(p * 1.2f - row * .1f, 0.f, 1.f);
                const int width = static_cast<int>((1 - local) * 1280);
                const int x = transitionBackwards_ ? 0 : 1280 - width;
                if (width > 0) {
                    beginLogicalScissor(x, row * 240, width, 240);
                    drawLogicalTexture(outgoing_.texture, WHITE);
                    DrawRectangle(x, row * 240, 4, 240, Palette::Teal);
                    EndScissorMode();
                }
            }
        }
    }
#ifndef NDEBUG
    DrawFPS(16, 16);
#endif
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    const auto viewport = Letterbox::fit(GetScreenWidth(), GetScreenHeight());
    if (viewport.width > 0.0f && viewport.height > 0.0f) {
        DrawTexturePro(surface_.texture,
                       {0, 0, static_cast<float>(surface_.texture.width),
                        -static_cast<float>(surface_.texture.height)},
                       {viewport.x, viewport.y, viewport.width, viewport.height}, {0, 0}, 0, WHITE);
    }
    EndDrawing();
}

void Renderer::drawPlaceholder(const char* title, const char* subtitle, const UiAssets* assets) {
    if (assets) {
        const float width = MeasureTextEx(assets->title(), title, 40, 1).x;
        const float caption = MeasureTextEx(assets->body(), subtitle, 20, 1).x;
        assets->text(title, {(1280 - width) * .5f, 288}, 40, Palette::Bone, false, true);
        assets->text(subtitle, {(1280 - caption) * .5f, 360}, 20, Palette::Teal, true);
        assets->text("F11 fullscreen", {24, 680}, 18, Palette::Bone, true);
        return;
    }
    constexpr Color kBone = {233, 228, 208, 255};
    constexpr Color kTeal = {63, 143, 140, 255};
    DrawText(title, (Letterbox::kWidth - MeasureText(title, 40)) / 2, 288, 40, kBone);
    DrawText(subtitle, (Letterbox::kWidth - MeasureText(subtitle, 20)) / 2, 360, 20, kTeal);
    DrawText("F11 fullscreen", 24, Letterbox::kHeight - 40, 18, kBone);
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
                         const EnemyCombatSystem* enemyCombat, const AlarmSequence* alarmSequence,
                         const ObjectiveSystem* objectives) {
    constexpr Color kBone = {233, 228, 208, 255};
    constexpr Color kGold = {242, 183, 5, 255};
    constexpr Color kAlarm = {255, 59, 92, 255};
    const float flip = pickupsLit ? (alarmSequence ? alarmSequence->paletteBlend() : 1.f) : 0.f;
    const Color wallColor = ColorLerp(Palette::Teal, Palette::Alarm, flip);
    const Color floorColor = ColorLerp(Palette::DeepTeal, Palette::DarkAlarm, flip);
    alarmPulse_ = alarmSequence && !reduceEffects_ ? alarmSequence->vignettePulse() : 0.f;
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
    if (alarmSequence) {
        const auto offset = alarmSequence->shakeOffset(reduceEffects_);
        camera.offset.x += offset.x;
        camera.offset.y += offset.y;
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
            const float visible = std::max(reveal, config_.ambientWallAlpha);
            if (tile == TileType::Wall) {
                // Structural mass remains dark. Light catches only the room-facing caps.
                DrawRectangleRec({x * size, y * size, size, size}, Palette::Ink);
                const Color edge = Fade(wallColor, visible);
                const Color face = Fade(ColorLerp(wallColor, Palette::Ink, .52f), visible);
                const Color shine = Fade(ColorLerp(wallColor, Palette::Bone, .2f), visible);
                const float depth = size * .19f;
                if (pickupsLit && map.isPassable(x - 1, y) && map.isPassable(x + 1, y) &&
                    map.isPassable(x, y - 1) && map.isPassable(x, y + 1)) {
                    DrawRectangleRec({x * size + 3, y * size + 3, size - 6, size - 6},
                                     ColorLerp(floorColor, Palette::Ink, .65f));
                    DrawRectangleRec({x * size + 6, y * size + 5, size - 12, size - 16}, face);
                    DrawRectangleRec({x * size + 8, y * size + 7, size - 16, 2}, edge);
                }
                if (map.isPassable(x - 1, y)) {
                    DrawRectangleRec({x * size, y * size, depth, size}, face);
                    DrawRectangleRec({x * size, y * size, 2, size}, shine);
                    DrawRectangleRec({x * size + depth - 2, y * size, 2, size}, edge);
                }
                if (map.isPassable(x + 1, y)) {
                    DrawRectangleRec({(x + 1) * size - depth, y * size, depth, size}, face);
                    DrawRectangleRec({(x + 1) * size - 2, y * size, 2, size}, shine);
                    DrawRectangleRec({(x + 1) * size - depth, y * size, 2, size}, edge);
                }
                if (map.isPassable(x, y - 1)) {
                    DrawRectangleRec({x * size, y * size, size, depth}, face);
                    DrawRectangleRec({x * size, y * size, size, 2}, shine);
                    DrawRectangleRec({x * size, y * size + depth - 2, size, 2}, edge);
                }
                if (map.isPassable(x, y + 1)) {
                    DrawRectangleRec({x * size, (y + 1) * size - depth, size, depth}, face);
                    DrawRectangleRec({x * size, (y + 1) * size - 2, size, 2}, shine);
                    DrawRectangleRec({x * size, (y + 1) * size - depth, size, 2}, edge);
                }
            } else {
                const float lightVisibility = map.light(x, y) == LightLevel::Lit   ? 1.f
                                              : map.light(x, y) == LightLevel::Dim ? .35f
                                                                                   : 0.f;
                const float floorVisibility =
                    overview || pickupsLit ? 1.f
                                           : std::max(lightVisibility, config_.ambientFloorAlpha);
                DrawRectangleRec({x * size, y * size, size, size},
                                 Fade(floorColor, floorVisibility));
                // Low-contrast polished panels remain subordinate to actors and pings.
                DrawRectangleLinesEx(
                    {x * size + .5f, y * size + .5f, size - 1, size - 1}, .5f,
                    Fade(ColorLerp(floorColor, Palette::Bone, .08f), floorVisibility * .35f));
            }
            if (reveal > 0 && tile == TileType::Bollard) {
                const auto point = map.tileCenter({x, y});
                if (map.isOpen(x, y))
                    DrawCircleLinesV({point.x, point.y}, size * .16f, Fade(kBone, reveal * .25f));
                else {
                    DrawCircleV({point.x, point.y}, size * .16f, Fade(kBone, reveal));
                    DrawCircleV({point.x, point.y}, size * .08f, Fade(kGold, reveal));
                }
            }
            if (reveal > 0 && !isDoor(tile) && tile != TileType::Bollard &&
                tile != TileType::Wall && tile != TileType::Floor && !map.isOpen(x, y) &&
                tile != TileType::PlayerSpawn && tile != TileType::Keycard &&
                !(objectives && (tile == TileType::Money || tile == TileType::VaultDoor ||
                                 tile == TileType::VanSpawn))) {
                const auto center = map.tileCenter({x, y});
                const auto color = map.isPassable(x, y) ? kGold : kBone;
                if (tile == TileType::SecurityPanel || tile == TileType::Breaker ||
                    tile == TileType::BollardPanel) {
                    const Rectangle cabinet{center.x - size * .23f, center.y - size * .32f,
                                            size * .46f, size * .64f};
                    DrawRectangleRounded(cabinet, .08f, 12, Fade(Palette::Ink, reveal));
                    DrawRectangleLinesEx(cabinet, 1, Fade(kBone, reveal * .65f));
                    DrawRectangleRec({cabinet.x + 3, cabinet.y + 3, cabinet.width - 6, size * .28f},
                                     Fade(ColorLerp(wallColor, floorColor, .6f), reveal));
                    for (int line = 0; line < 3; ++line)
                        DrawLineEx(
                            {cabinet.x + 5, cabinet.y + 6 + line * size * .045f},
                            {cabinet.x + cabinet.width - 5, cabinet.y + 6 + line * size * .045f}, 1,
                            Fade(Palette::Teal, reveal));
                    for (int key = 0; key < 3; ++key)
                        DrawRectangleRec({cabinet.x + 4 + key * size * .12f,
                                          cabinet.y + size * .43f, size * .07f, size * .06f},
                                         Fade(key == 2 ? kGold : kBone, reveal));
                } else {
                    DrawRectangleRec(
                        {center.x - size * .2f, center.y - size * .2f, size * .4f, size * .4f},
                        Fade(color, reveal));
                }
                if (overview) {
                    const char symbol[] = {static_cast<char>(tile), '\0'};
                    uiAssets_.text(symbol,
                                   {static_cast<float>(static_cast<int>(x * size + size * 0.25f)),
                                    static_cast<float>(static_cast<int>(y * size + size * 0.25f))},
                                   size * 0.5f, {10, 10, 12, 255}, true);
                }
            }
            if (!overview && !pickupsLit) {
                const auto center = map.tileCenter({x, y});
                const bool halo = std::hypot(center.x - player.pos.x, center.y - player.pos.y) <=
                                      ripple.haloRadius() + size &&
                                  Raycast::hasLineOfSight(player.pos, center, map, true);
                if (halo) {
                    beginLogicalScissor(static_cast<int>(std::floor(topLeft.x)),
                                        static_cast<int>(std::floor(topLeft.y)),
                                        static_cast<int>(std::ceil(screenSize)),
                                        static_cast<int>(std::ceil(screenSize)));
                    if (halo && tile != TileType::Wall) {
                        DrawCircleGradient(static_cast<int>(spawn.x), static_cast<int>(spawn.y),
                                           ripple.haloRadius(), floorColor, Fade(floorColor, 0));
                    }
                    BeginBlendMode(BLEND_ADDITIVE);
                    if (halo)
                        DrawCircleGradient(static_cast<int>(spawn.x), static_cast<int>(spawn.y),
                                           ripple.haloRadius(), Fade(kBone, 0.12f), Fade(kBone, 0));
                    EndBlendMode();
                    EndScissorMode();
                }
            }
        }
    }
    bool pingVisible = ripple.waveActive();
    if (!overview && !pickupsLit)
        for (int y = 0; y < map.height() && !pingVisible; ++y)
            for (int x = 0; x < map.width() && !pingVisible; ++x)
                pingVisible = ripple.tileReveal(x, y) > 0;
    if (!overview && !pickupsLit && pingVisible && ripple.waveRadius() > 0) {
        const auto origin = ripple.origin();
        const Vector2 eye{origin.x, origin.y};
        const float radius = ripple.waveRadius();
        sightBoundary(origin, radius, map, pingAngles_, pingBoundary_);
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x) {
                if (map.tile(x, y) == TileType::Wall) continue;
                const auto screen = GetWorldToScreen2D({x * size, y * size}, camera);
                if (screen.x + size < 0 || screen.y + size < 0 || screen.x >= 1280 ||
                    screen.y >= 720)
                    continue;
                float timed = ripple.tileReveal(x, y);
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        timed = std::max(timed, ripple.tileReveal(x + dx, y + dy));
                const float light = map.light(x, y) == LightLevel::Lit   ? 1.f
                                    : map.light(x, y) == LightLevel::Dim ? .35f
                                                                         : 0.f;
                const float base = std::max(light, config_.ambientFloorAlpha);
                const float opacity = base < 1 ? std::max(0.f, (timed - base) / (1 - base)) : 0.f;
                if (opacity <= 0 && !ripple.waveActive()) continue;
                const Rectangle tile{x * size, y * size, size, size};
                const float centerX = (x + .5f) * size - origin.x;
                const float centerY = (y + .5f) * size - origin.y;
                const float distance = std::hypot(centerX, centerY);
                const float bound = size * .707107f;
                if (distance > radius + bound) continue;
                const bool nearEye = distance <= bound;
                const float center = std::atan2(centerY, centerX);
                const float span = nearEye ? kPi : std::asin(bound / distance);
                for (int wrap = -1; wrap <= 1; ++wrap) {
                    if (nearEye && wrap != 0) continue;
                    const float low = nearEye ? 0 : center - span + wrap * 2 * kPi;
                    const float high = nearEye ? 2 * kPi : center + span + wrap * 2 * kPi;
                    const auto first = std::max<std::size_t>(
                        1, std::lower_bound(pingAngles_.begin(), pingAngles_.end(), low) -
                               pingAngles_.begin());
                    const auto last = std::min<std::size_t>(
                        pingAngles_.size() - 1,
                        std::upper_bound(pingAngles_.begin(), pingAngles_.end(), high) -
                            pingAngles_.begin());
                    const auto paint = [&](Color color) {
                        for (std::size_t i = first; i <= last; ++i)
                            drawClippedTriangle(eye, pingBoundary_[i], pingBoundary_[i - 1], tile,
                                                color);
                    };
                    if (opacity > 0) paint(Fade(floorColor, opacity));
                    if (ripple.waveActive()) {
                        BeginBlendMode(BLEND_ADDITIVE);
                        paint(Fade(kBone, .08f));
                        EndBlendMode();
                    }
                }
            }
        if (ripple.waveActive())
            for (std::size_t i = 1; i < pingBoundary_.size(); ++i) {
                const auto a = pingBoundary_[i - 1], b = pingBoundary_[i];
                if (std::hypot(a.x - eye.x, a.y - eye.y) < radius - .01f ||
                    std::hypot(b.x - eye.x, b.y - eye.y) < radius - .01f)
                    continue;
                DrawLineEx(a, b, 2, Fade(kBone, .9f * (1 - radius / ripple.maxRadius())));
            }
    }
    if (level.name == "Gotham Central Bank") {
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x) {
                if (map.tile(x, y) != TileType::Floor) continue;
                const auto screen = GetWorldToScreen2D({x * size, y * size}, camera);
                if (screen.x + size < 0 || screen.y + size < 0 || screen.x >= 1280 ||
                    screen.y >= 720)
                    continue;
                const float reveal =
                    overview || pickupsLit ? 1.f : ripple.visibility(x, y, player.pos, map);
                DrawRectangleLinesEx({x * size + .5f, y * size + .5f, size - 1, size - 1}, .6f,
                                     Fade(ColorLerp(floorColor, Palette::Ink, .55f),
                                          std::max(reveal, config_.ambientFloorAlpha) * .5f));
            }
    }
    // Contact shadows sit on the room floor after the floor pass. They describe
    // depth without changing collision, light-zone classification or reveal.
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            if (map.tile(x, y) != TileType::Wall) continue;
            const auto screen = GetWorldToScreen2D({x * size, y * size}, camera);
            if (screen.x + size * 2 < 0 || screen.y + size * 2 < 0 || screen.x - size >= 1280 ||
                screen.y - size >= 720)
                continue;
            const float reveal =
                overview || pickupsLit ? 1.f : ripple.visibility(x, y, player.pos, map);
            const float visibility = std::max(reveal, config_.ambientFloorAlpha);
            const Color shadow = Fade(BLACK, visibility * .47f);
            if (map.isPassable(x + 1, y))
                DrawRectangleRec({(x + 1) * size, y * size, size * .12f, size}, shadow);
            if (map.isPassable(x, y + 1))
                DrawRectangleRec({x * size, (y + 1) * size, size, size * .17f}, shadow);
        }
    if (level.name == "Gotham Central Bank") {
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x) {
                if (map.tile(x, y) != TileType::Floor && map.tile(x, y) != TileType::Keycard)
                    continue;
                const auto screen = GetWorldToScreen2D({x * size, y * size}, camera);
                if (screen.x + size * 2 < 0 || screen.y + size * 2 < 0 || screen.x - size >= 1280 ||
                    screen.y - size >= 720)
                    continue;
                const float reveal =
                    overview || pickupsLit ? 1.f : ripple.visibility(x, y, player.pos, map);
                drawBankFurniture(x, y, size, std::max(reveal, config_.ambientFloorAlpha),
                                  wallColor, floorColor);
            }
    }
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            if (!isDoor(map.tile(x, y)) && map.tile(x, y) != TileType::Keycard) continue;
            const auto screen = GetWorldToScreen2D({x * size, y * size}, camera);
            if (screen.x + size * 2 < 0 || screen.y + size * 2 < 0 || screen.x - size >= 1280 ||
                screen.y - size >= 720)
                continue;
            const float reveal =
                overview || pickupsLit ? 1.f : ripple.visibility(x, y, player.pos, map);
            if (reveal <= 0) continue;
            if (map.tile(x, y) == TileType::Keycard) {
                const auto center = map.tileCenter({x, y});
                drawKeycard(center, size, reveal);
                if (overview)
                    uiAssets_.text("k", {center.x - size * .25f, center.y - size * .25f},
                                   size * .5f, Palette::Ink, true);
            } else
                drawBankDoor(x, y, size, reveal, map, wallColor);
        }
    if (objectives)
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                const auto tile = map.tile(x, y);
                const float reveal =
                    overview || pickupsLit ? 1.f : ripple.visibility(x, y, player.pos, map);
                if (objectives && tile == TileType::VaultDoor &&
                    map.tile(x - 1, y) != TileType::VaultDoor) {
                    int span = 1;
                    float visible = reveal;
                    while (map.tile(x + span, y) == TileType::VaultDoor) {
                        visible = std::max(visible,
                                           overview || pickupsLit
                                               ? 1.f
                                               : ripple.visibility(x + span, y, player.pos, map));
                        ++span;
                    }
                    const float width = span * size;
                    const Rectangle frame{x * size + 3, y * size + 3, width - 6, size - 6};
                    const Vector2 center{x * size + width * 0.5f, y * size + size * 0.5f};
                    if (visible > 0) {
                        DrawRectangleLinesEx(frame, 3, Fade(kBone, visible));
                        if (!map.isOpen(x, y)) {
                            DrawRectangleRec(
                                {frame.x + 5, frame.y + 5, frame.width - 10, frame.height - 10},
                                Fade(wallColor, visible));
                            DrawCircleSector(center, size * .4f, 0, 360, 96,
                                             Fade(Palette::Ink, visible));
                            DrawRing(center, size * .32f, size * .39f, 0, 360, 96,
                                     Fade(ColorLerp(wallColor, kBone, .35f), visible));
                            DrawRing(center, size * .29f, size * .31f, 0, 360, 96,
                                     Fade(kBone, visible));
                            DrawCircleSector(
                                center, size * .26f, 0, 360, 96,
                                Fade(ColorLerp(wallColor, Palette::Ink, .7f), visible));
                            for (int bolt = 0; bolt < 12; ++bolt) {
                                const float angle = bolt * 2 * kPi / 12;
                                DrawCircleSector({center.x + std::cos(angle) * size * .35f,
                                                  center.y + std::sin(angle) * size * .35f},
                                                 size * .012f, 0, 360, 16,
                                                 Fade(Palette::Ink, visible));
                            }
                            DrawCircleSector(center, size * .11f, 0, 360, 64, Fade(kGold, visible));
                            DrawRing(center, size * .045f, size * .065f, 0, 360, 32,
                                     Fade(kBone, visible));
                            for (int spoke = 0; spoke < 4; ++spoke) {
                                const float angle = spoke * kPi * 0.5f;
                                DrawLineEx(center,
                                           {center.x + std::cos(angle) * size * 0.26f,
                                            center.y + std::sin(angle) * size * 0.26f},
                                           2, Fade(kBone, visible));
                            }
                        } else {
                            DrawRectangleRec({frame.x, frame.y, 8, frame.height},
                                             Fade(kGold, visible));
                            DrawRectangleRec({frame.x + frame.width - 8, frame.y, 8, frame.height},
                                             Fade(kGold, visible));
                        }
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
                uiAssets_.text(guard.id.c_str(),
                               {static_cast<float>(static_cast<int>(position.x + guard.radius)),
                                static_cast<float>(static_cast<int>(position.y))},
                               24, kBone, true);
            continue;
        }
        if (!pickupsLit) drawGuardCone(guard, position, map, player.isCrouched(), visible);
        drawOperator({position.x, position.y}, guard.radius, guard.facing(), Palette::Navy, visible,
                     pickupsLit, false);
        const Vector2 direction{std::cos(guard.facing()), std::sin(guard.facing())};
        DrawLineEx({position.x, position.y},
                   {position.x + direction.x * guard.radius * 1.7f,
                    position.y + direction.y * guard.radius * 1.7f},
                   guard.radius * 0.25f, Fade(kGold, visible));
        if (overview)
            uiAssets_.text(guard.id.c_str(),
                           {static_cast<float>(static_cast<int>(position.x + guard.radius)),
                            static_cast<float>(static_cast<int>(position.y))},
                           24, kBone, true);
        if (guard.detection() > 0) {
            const Vector2 meter{position.x, position.y - guard.radius - 12};
            DrawCircleV(meter, 8, Fade({20, 22, 27, 255}, visible));
            DrawCircleSector(meter, 8, -90, -90 + 360 * guard.detection() / 100, 32,
                             Fade(kAlarm, visible));
            DrawCircleLinesV(meter, 8, Fade(kBone, visible));
            if (guard.state() == GuardState::Alerted)
                uiAssets_.text(TextFormat("%.1fs", guard.callInRemaining()),
                               {static_cast<float>(static_cast<int>(meter.x + 12)),
                                static_cast<float>(static_cast<int>(meter.y - 8))},
                               16, Fade(kAlarm, visible), true);
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
        drawOperator({p.x, p.y}, enemy->radius, enemy->facing(), body, visible, pickupsLit, false,
                     heavy);
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
    if (objectives && objectives->vanArrived()) {
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x)
                if (map.tile(x, y) == TileType::VanSpawn) {
                    const auto p = map.tileCenter({x, y});
                    DrawRectangleRounded({p.x - 44, p.y - 24, 88, 48}, .18f, 8, {20, 22, 27, 255});
                    DrawRectangleRoundedLinesEx({p.x - 44, p.y - 24, 88, 48}, .18f, 8, 2, kBone);
                    DrawRectangleRec({p.x + 18, p.y - 18, 16, 36}, {63, 143, 140, 255});
                    DrawCircleV({p.x - 24, p.y - 24}, 6, kBone);
                    DrawCircleV({p.x - 24, p.y + 24}, 6, kBone);
                }
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
    if (objectives) {
        for (const auto& bag : objectives->bags()) {
            if (bag.state == BagState::Delivered) continue;
            const bool carried = bag.state == BagState::Carried;
            const Vec2 position = carried ? Vec2{spawn.x - std::cos(facing) * player.radius,
                                                 spawn.y - std::sin(facing) * player.radius}
                                          : bag.pos;
            const float visible =
                overview || pickupsLit || carried
                    ? 1.f
                    : std::max(bag.reveal, ripple.visibility(static_cast<int>(position.x / size),
                                                             static_cast<int>(position.y / size),
                                                             player.pos, map));
            if (visible <= 0) continue;
            const Color gold = Fade(kGold, visible), bone = Fade(kBone, visible);
            if (bag.state == BagState::Stack) {
                for (int layer = 0; layer < 3; ++layer) {
                    const Rectangle cash{position.x - 15 + layer * 2.f,
                                         position.y - 7 - layer * 3.f, 28, 14};
                    DrawRectangleRec(cash, Fade(Color{20, 22, 27, 255}, visible));
                    DrawRectangleLinesEx(cash, 2, gold);
                    DrawRectangleRec({cash.x + 11, cash.y, 5, cash.height}, bone);
                }
                if (bag.dye == DyeState::Armed)
                    DrawCircleV({position.x + 15, position.y - 15}, 3, Fade(kAlarm, visible));
            } else {
                DrawEllipse(static_cast<int>(position.x), static_cast<int>(position.y + 3), 13, 15,
                            gold);
                DrawTriangle({position.x - 7, position.y - 12}, {position.x, position.y - 6},
                             {position.x + 7, position.y - 12}, gold);
                DrawLineEx({position.x - 7, position.y - 6}, {position.x + 7, position.y - 6}, 3,
                           bone);
            }
            if (bag.dye == DyeState::Spoiled) {
                DrawLineEx({position.x - 5, position.y - 2}, {position.x + 5, position.y + 8}, 3,
                           Fade(kAlarm, visible));
                if (bag.burstAge < 0.4f && !reduceEffects_)
                    DrawCircleV({position.x, position.y}, 10 + bag.burstAge * 60,
                                Fade(kAlarm, visible * (1 - bag.burstAge / 0.4f) * 0.4f));
            }
        }
        if (objectives->thermiteRemaining() > 0) {
            const auto position = objectives->vaultPosition();
            const float age = objectives->thermiteAge();
            const float pulse = 0.5f + 0.5f * std::sin(age * 2.f * 2.f * kPi);
            DrawCircleGradient(static_cast<int>(position.x), static_cast<int>(position.y), 36,
                               Fade(kGold, 0.3f + pulse * 0.2f), Fade(kGold, 0));
            DrawRectangleRounded({position.x - 10, position.y - 8, 20, 16}, 0.25f, 4, kGold);
            DrawRectangleLinesEx({position.x - 10, position.y - 8, 20, 16}, 2, kBone);
            if (!reduceEffects_)
                for (int spark = 0; spark < 6; ++spark) {
                    const float phase = std::fmod(age * 2 + spark / 6.f, 1.f);
                    const float angle = spark * 2.4f + std::floor(age * 2) * 0.7f;
                    const Vector2 tip{position.x + std::cos(angle) * phase * 30,
                                      position.y + std::sin(angle) * phase * 30};
                    DrawLineEx(tip, {tip.x + std::cos(angle) * 5, tip.y + std::sin(angle) * 5}, 2,
                               Fade(kGold, 1 - phase));
                }
            const int remaining = static_cast<int>(std::ceil(objectives->thermiteRemaining()));
            uiAssets_.text(TextFormat("THERMITE %d:%02d", remaining / 60, remaining % 60),
                           {static_cast<float>(static_cast<int>(position.x - 64)),
                            static_cast<float>(static_cast<int>(
                                position.y + (player.pos.y > position.y ? -40 : 32)))},
                           16, kGold, true);
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
    drawOperator({spawn.x, spawn.y}, player.radius, facing, Palette::Ink, 1, pickupsLit, true);
    DrawLineEx({spawn.x, spawn.y},
               {spawn.x + std::cos(facing) * player.radius * 1.7f,
                spawn.y + std::sin(facing) * player.radius * 1.7f},
               3, kBone);
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

    uiAssets_.text(level.name.c_str(), {464, 24}, 18, kBone);
    uiAssets_.text(player.isCrouched()    ? "CROUCH"
                   : player.isSprinting() ? "SPRINT"
                                          : "WALK",
                   {464, 48}, 14, kGold, true);

#ifndef NDEBUG
    if (overview)
        uiAssets_.text(TextFormat("%d x %d | Seed %u", map.width(), map.height(), seed), {464, 72},
                       14, Palette::Teal, true);
#else
    (void)seed;
#endif
}

void Renderer::drawInteractionHud(const World& world, const InteractionSystem& interaction,
                                  float noiseRadius, float maximumNoise,
                                  const ObjectiveSystem* objectives) const {
    constexpr Color kBone = {233, 228, 208, 255};
    constexpr Color kTeal = {63, 143, 140, 255};
    constexpr Color kSlate = {20, 22, 27, 255};
    const char* objective =
        world.powerOn ? "Gate open: reach the vault corridor"
                      : (world.player.hasKeycard() ? "Find the breaker and restore gate power"
                                                   : "Enter the bank and find the red keycard");
    std::string words = objectives ? objectives->objective() : objective;
    std::istringstream stream(words);
    std::vector<std::string> lines;
    std::string word, line;
    while (stream >> word) {
        const auto candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureTextEx(uiAssets_.body(), candidate.c_str(), 18, 1).x > 194) {
            lines.push_back(line);
            line = word;
        } else
            line = candidate;
    }
    if (!line.empty()) lines.push_back(line);
    const float height = 48 + lines.size() * 22.f + (world.alarmLoud ? 0 : 24);
    DrawRectangleRounded({24, 80, 226, height}, .07f, 8, kSlate);
    DrawRectangleRoundedLinesEx({24, 80, 226, height}, .07f, 8, 2, Fade(kBone, .2f));
    uiAssets_.text("OBJECTIVE", {40, 96}, 14, kTeal);
    float y = 120;
    for (const auto& value : lines) {
        uiAssets_.text(value.c_str(), {40, y}, 18, kBone, true);
        y += 22;
    }
    if (objectives) {
        DrawRectangleRounded({1116, 24, 140, 54}, .2f, 8, kSlate);
        DrawRectangleRoundedLinesEx({1116, 24, 140, 54}, .2f, 8, 2, Fade(kBone, .2f));
        DrawRectangleRounded({1130, 40, 18, 22}, .3f, 8, {242, 183, 5, 255});
        DrawLine(1134, 38, 1144, 38, kBone);
        uiAssets_.text(TextFormat("%d / %d", objectives->deliveredCount(),
                                  static_cast<int>(objectives->bags().size())),
                       {1160, 40}, 20, kBone, false, true);
        uiAssets_.text(world.player.carryingBag() ? "G: THROW BAG" : "HANDS FREE", {1032, 88}, 14,
                       kBone, true);
        const char* van = objectives->vanArrived() ? "VAN READY"
                          : objectives->bollardsLowered()
                              ? TextFormat("VAN %.1fs", objectives->vanRemaining())
                              : "BOLLARDS UP";
        uiAssets_.text(van, {1032, 112}, 14, kTeal, true);
    }
    if (!world.alarmLoud) {
        const int filled =
            maximumNoise > 0
                ? std::clamp(static_cast<int>(std::ceil(noiseRadius / maximumNoise * 6)), 0, 6)
                : 0;
        uiAssets_.text("NOISE", {40, y + 4}, 14, kBone, true);
        for (int i = 0; i < 6; ++i)
            DrawRectangle(104 + i * 20, static_cast<int>(y + 4), 12, 12,
                          i < filled ? kTeal : Color{40, 44, 50, 255});
    }
    if (world.securityLoopRemaining > 0)
        uiAssets_.text(TextFormat("SECURITY LOOP %.0fs", world.securityLoopRemaining), {272, 96},
                       14, kTeal, true);
    if (const auto* target = interaction.target()) {
        DrawRectangle(312, Letterbox::kHeight - 208, 704, 56, kSlate);
        uiAssets_.text(target->prompt.c_str(),
                       {static_cast<float>(336), static_cast<float>(Letterbox::kHeight - 192)}, 20,
                       kBone, true);
        if (interaction.progress() > 0)
            DrawRing({960, static_cast<float>(Letterbox::kHeight - 180)}, 14, 18, -90,
                     -90 + 360 * interaction.progress(), 64, kBone);
    }
}

void Renderer::drawPickupHud(const RecoveryPickup* pickup) const {
    if (!pickup) return;
    DrawRectangle(312, Letterbox::kHeight - 208, 704, 56, {20, 22, 27, 255});
    uiAssets_.text(
        pickup->type == PickupType::Medkit ? "E: collect medkit" : "E: collect armor plate",
        {static_cast<float>(336), static_cast<float>(Letterbox::kHeight - 192)}, 20,
        {233, 228, 208, 255}, true);
}

void Renderer::drawStealthHud(const AlarmDirector& alarm, const PagerSystem& pagers,
                              const std::vector<Guard>& guards) const {
    constexpr Color kAlarm = {255, 59, 92, 255};
    constexpr Color kBone = {233, 228, 208, 255};
    if (alarm.state() == AlarmState::CallIn)
        uiAssets_.text(TextFormat("SPOTTED %.1fs", alarm.callInRemaining()),
                       {static_cast<float>(560), static_cast<float>(96)}, 24, kAlarm, true);
    else if (alarm.state() == AlarmState::Loud)
        uiAssets_.text("ALARM", {static_cast<float>(560), static_cast<float>(96)}, 24, kAlarm,
                       true);
    int y = 136;
    for (const auto& guard : guards)
        if (pagers.state(guard.id) == PagerState::Ringing) {
            uiAssets_.text(
                TextFormat("PAGER %s %.1fs", guard.id.c_str(), pagers.remaining(guard.id)),
                {static_cast<float>(560), static_cast<float>(y)}, 18, kBone, true);
            y += 24;
        }
}

void Renderer::drawWaveHud(const WaveSpawner& waves) const {
    constexpr Color kBone{233, 228, 208, 255};
    constexpr Color kGold{242, 183, 5, 255};
    DrawRectangleRounded({1010, 160, 246, 88}, 0.1f, 4, {20, 22, 27, 255});
    DrawRectangleLinesEx({1010, 160, 246, 88}, 1, Fade(kBone, 0.65f));
    uiAssets_.text(TextFormat("ASSAULT  %d", waves.waveIndex()), {1026, 176}, 20, kGold, false,
                   true);
    uiAssets_.text(TextFormat("NEXT WAVE  %.0fs", std::ceil(waves.nextWaveRemaining())),
                   {1026, 210}, 18, kBone, true);
}
void Renderer::drawWeaponHud(const CombatSystem& combat) const {
    const auto& weapon = combat.activeWeapon();
    constexpr Color kBone{233, 228, 208, 255};
    constexpr Color kGold{242, 183, 5, 255};
    DrawRectangle(24, 550, 272, 146, {20, 22, 27, 255});
    DrawRectangleLinesEx({24, 550, 272, 146}, 1, Fade(kBone, 0.6f));
    const std::string name = std::to_string(combat.activeSlot() + 1) + "  " + weapon.spec().name;
    uiAssets_.text(name.c_str(), {40, 562}, 20, kBone, false, true);
    const std::string ammunition =
        std::to_string(weapon.ammunition()) + " / " + std::to_string(weapon.reserve());
    uiAssets_.text(ammunition.c_str(), {40, 594}, 28, kGold, false, true);
    if (weapon.reloadRemaining() > 0) {
        uiAssets_.text("RELOADING", {static_cast<float>(158), static_cast<float>(600)}, 16, kBone,
                       true);
        const float progress = 1 - weapon.reloadRemaining() / weapon.spec().reload;
        DrawRectangle(40, 624, static_cast<int>(232 * progress), 3, kGold);
    }
    if (!healthHud_.initialized()) return;
    const auto meter = [this](float shown, float maximum, float y, Color color, const char* label) {
        uiAssets_.text(label, {40, y - 16}, 14, {233, 228, 208, 255}, true);
        DrawRectangleRounded({40, y, 240, 12}, 1, 8, {40, 44, 50, 255});
        const float fraction = maximum > 0 ? std::clamp(shown / maximum, 0.0f, 1.0f) : 0;
        if (fraction > 0) DrawRectangleRounded({40, y, 240 * fraction, 12}, 1, 8, color);
    };
    meter(healthHud_.displayedHp(), healthHud_.maximumHp(), 646, {255, 59, 92, 255}, "HEALTH");
    meter(healthHud_.displayedArmor(), healthHud_.maximumArmor(), 676, kBone, "ARMOR");
}

void Renderer::drawAlarmSequence(const AlarmSequence& sequence) const {
    const int height = static_cast<int>(std::round(sequence.barsFraction() * 48));
    if (height > 0) {
        DrawRectangle(0, 0, Letterbox::kWidth, height, {10, 10, 12, 255});
        DrawRectangle(0, Letterbox::kHeight - height, Letterbox::kWidth, height, {10, 10, 12, 255});
    }
    if (sequence.bannerVisible()) {
        DrawRectangle(0, 16, Letterbox::kWidth, 48, Palette::Alarm);
        const char* message = "POLICE INBOUND";
        uiAssets_.text(
            message,
            {static_cast<float>(
                 (Letterbox::kWidth - MeasureTextEx(uiAssets_.body(), message, 28, 1).x) / 2),
             static_cast<float>(26)},
            28, Palette::Bone, true);
    }
}

void Renderer::drawBusted(int stage) const {
    DrawRectangle(0, 0, Letterbox::kWidth, Letterbox::kHeight, Fade(Color{20, 22, 27, 255}, 0.85f));
    DrawRectangleRec({360, 240, 560, 240}, {20, 22, 27, 255});
    DrawRectangleLinesEx({360, 240, 560, 240}, 2, Palette::Bone);
    uiAssets_.text("BUSTED", {static_cast<float>(520), static_cast<float>(272)}, 48, Palette::Alarm,
                   true);
    uiAssets_.text(TextFormat("Retry from S%d", stage),
                   {static_cast<float>(520), static_cast<float>(344)}, 24, Palette::Bone, true);
    uiAssets_.text("ENTER: RETRY", {static_cast<float>(520), static_cast<float>(402)}, 24,
                   Color{242, 183, 5, 255}, true);
}

void Renderer::drawLoadout(int excluded, bool easy, const UiAssets* assets) {
    const auto text = [&](const char* value, Vector2 p, float size, Color color, bool body) {
        if (assets)
            assets->text(value, p, size, color, body);
        else
            DrawText(value, static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(size),
                     color);
    };
    constexpr Color bone{233, 228, 208, 255}, gold{242, 183, 5, 255};
    DrawRectangle(160, 80, 960, 560, {20, 22, 27, 255});
    text("LOADOUT", {static_cast<float>(200), static_cast<float>(112)}, 36, bone, true);
    text("Carry two guns. Press 1, 2 or 3 to leave one behind.",
         {static_cast<float>(200), static_cast<float>(176)}, 22, bone, true);
    const char* names[] = {"1  WHISPER - suppressed pistol", "2  CHATTER - SMG",
                           "3  GAVEL - shotgun"};
    for (int i = 0; i < 3; ++i) {
        text(names[i], {static_cast<float>(200), static_cast<float>(248 + i * 64)}, 24,
             i == excluded ? Color{133, 133, 133, 255} : gold, true);
        text(i == excluded ? "LEAVE" : "EQUIPPED",
             {static_cast<float>(880), static_cast<float>(248 + i * 64)}, 20, bone, true);
    }
    text(easy ? "C: difficulty TOURIST (Easy)" : "C: difficulty PROFESSIONAL (Normal)",
         {static_cast<float>(200), static_cast<float>(472)}, 24, bone, true);
    text("ENTER: start heist    ESC: back to briefing",
         {static_cast<float>(200), static_cast<float>(568)}, 24, gold, true);
}
void Renderer::drawPayout(const Payout& payout) {
    constexpr Color bone{233, 228, 208, 255}, gold{242, 183, 5, 255};
    DrawRectangle(240, 32, 800, 656, {20, 22, 27, 255});
    DrawText("HEIST COMPLETE", 280, 64, 36, bone);
    int y = 136;
    const auto line = [&](const char* label, double value) {
        DrawText(label, 280, y, 22, bone);
        DrawText(TextFormat("$%.0f", value), 816, y, 22, gold);
        y += 40;
    };
    line("Delivered cash", payout.subtotal);
    line("Ghost bonus", payout.ghostBonus);
    line("Time bonus", payout.timeBonus);
    line("Handler's cut", -payout.handlerCut);
    const char* quips[] = {"Van air freshener", "Emotional support coffee",
                           "Suspicious parking fee"};
    for (std::size_t i = 0; i < payout.deductions.size(); ++i)
        line(quips[i % 3], -payout.deductions[i]);
    line("Deaths", -payout.deathPenalty);
    DrawText(TextFormat("RANK %c    PAYOUT $%.0f", payout.rank, payout.finalAmount), 280, 556, 28,
             gold);
    DrawText("ENTER: return to menu", 280, 632, 22, bone);
}

void Renderer::drawVoice(const VoiceDirector& director, const UiConfig& config) const {
    if (!director.hint().empty() && hints_) {
        const float fraction = std::clamp(director.hintAge() / config.transitionTime, 0.f, 1.f);
        const float entry =
            reduceEffects_ ? 0 : (1 - fraction) * (1 - fraction) * (1 - fraction) * 520;
        const Rectangle toast{736 + entry, 272, 520, 72};
        DrawRectangleRounded(toast, .1f, 8, Fade(Palette::Ink, .9f));
        DrawRectangleRoundedLinesEx(toast, .1f, 8, 2, Fade(Palette::Bone, .2f));
        uiAssets_.text(director.hint().c_str(), {752 + entry, 296}, 18, Palette::Bone, true);
    }
    const auto* line = director.current();
    if (!line) return;
    std::istringstream words(line->text);
    std::vector<std::string> lines;
    std::string word, row;
    while (words >> word) {
        const auto next = row.empty() ? word : row + " " + word;
        if (!row.empty() && MeasureTextEx(uiAssets_.body(), next.c_str(), 22, 1).x > 664) {
            lines.push_back(row);
            row = word;
        } else
            row = next;
    }
    if (!row.empty()) lines.push_back(row);
    const float height = std::max(96.f, 32 + static_cast<float>(lines.size()) * 28);
    const float top = 696 - height;
    DrawRectangleRounded({312, top, 832, height}, .08f, 8, Fade(Palette::Ink, .8f));
    DrawRectangleRoundedLinesEx({312, top, 832, height}, .08f, 8, 2, Fade(Palette::Bone, .2f));
    const float flicker =
        line->priority == 3 && !reduceEffects_
            ? .6f + .4f * (.5f + .5f * std::cos(director.age() * config.panicFlickerHz * 2 * kPi))
            : 1.f;
    const Rectangle portrait{312, top, 96, 96};
    const auto texture = uiAssets_.handlerPortrait();
    if (texture.id)
        DrawTexturePro(
            texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            portrait, {0, 0}, 0, Fade(WHITE, flicker));
    else {
        DrawCircleV({360, top + 44}, 24, Palette::Bone);
        DrawRing({360, top + 44}, 26, 30, 170, 350, 24, Palette::Teal);
    }
    DrawRectangleLinesEx(portrait, 2, Fade(Palette::Bone, flicker));
    float y = top + 16;
    for (const auto& value : lines) {
        uiAssets_.text(value.c_str(), {432, y}, 22, Palette::Bone, true);
        y += 28;
    }
}

void Renderer::drawGhostPortrait(Rectangle bounds) {
    if (tactical_) tactical_->drawGhostPortrait(bounds);
}
