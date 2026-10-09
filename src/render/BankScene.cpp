#include "render/BankScene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "core/Logger.h"
#include "entities/Enemy.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "render/Palette.h"
#include "systems/ObjectiveSystem.h"
#include "systems/RippleSystem.h"
#include "world/Raycast.h"
#include "world/World.h"

namespace {
Matrix meshTransform(Vector3 scale, Vector3 position, bool uprightDisc = false) {
    Matrix result{};
    result.m0 = scale.x;
    if (uprightDisc) {
        result.m6 = scale.y;
        result.m9 = -scale.z;
    } else {
        result.m5 = scale.y;
        result.m10 = scale.z;
    }
    result.m12 = position.x;
    result.m13 = position.y;
    result.m14 = position.z;
    result.m15 = 1;
    return result;
}
struct Geometry {
    std::vector<float> positions, normals;
    void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 normal) {
        for (const auto p : {a, b, c, a, c, d}) {
            positions.insert(positions.end(), {p.x, p.y, p.z});
            normals.insert(normals.end(), {normal.x, normal.y, normal.z});
        }
    }
    Mesh upload() const {
        if (positions.empty()) return {};
        Mesh mesh{};
        mesh.vertexCount = static_cast<int>(positions.size() / 3);
        mesh.triangleCount = mesh.vertexCount / 3;
        const auto bytes = static_cast<unsigned int>(positions.size() * sizeof(float));
        mesh.vertices = static_cast<float*>(MemAlloc(bytes));
        mesh.normals = static_cast<float*>(MemAlloc(bytes));
        mesh.texcoords = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
        if (!mesh.vertices || !mesh.normals || !mesh.texcoords) {
            MemFree(mesh.vertices);
            MemFree(mesh.normals);
            MemFree(mesh.texcoords);
            throw std::runtime_error("Unable to allocate bank geometry");
        }
        std::memcpy(mesh.vertices, positions.data(), bytes);
        std::memcpy(mesh.normals, normals.data(), bytes);
        std::memset(mesh.texcoords, 0,
                    static_cast<std::size_t>(mesh.vertexCount) * 2 * sizeof(float));
        UploadMesh(&mesh, false);
        return mesh;
    }
};

bool doorTile(TileType tile) {
    return tile == TileType::Door || tile == TileType::ServiceDoor || tile == TileType::CardDoor ||
           tile == TileType::Gate || tile == TileType::FrontDoor || tile == TileType::VaultDoor;
}

unsigned char channel(float value) {
    return static_cast<unsigned char>(std::clamp(value, 0.f, 1.f) * 255.f);
}
}  // namespace

BankScene::BankScene(Logger& logger) {
#ifdef __EMSCRIPTEN__
    const char* vertex = "assets/shaders/glsl100/bank.vs";
    const char* fragment = "assets/shaders/glsl100/bank.fs";
#else
    const char* vertex = "assets/shaders/glsl330/bank.vs";
    const char* fragment = "assets/shaders/glsl330/bank.fs";
#endif
    material_ = LoadMaterialDefault();
    material_.shader = LoadShader(vertex, fragment);
    mapSizeLoc_ = GetShaderLocation(material_.shader, "mapSize");
    tileSizeLoc_ = GetShaderLocation(material_.shader, "tileSize");
    phaseLoc_ = GetShaderLocation(material_.shader, "phase");
    modeLoc_ = GetShaderLocation(material_.shader, "surfaceMode");
    haloLoc_ = GetShaderLocation(material_.shader, "haloRadius");
    playerLoc_ = GetShaderLocation(material_.shader, "playerPosition");
    if (mapSizeLoc_ < 0 || modeLoc_ < 0) {
        logger.log(LogLevel::Error, "Perspective bank shader unavailable");
        UnloadMaterial(material_);
        throw std::runtime_error("Perspective bank shader unavailable");
    }
    cube_ = GenMeshCube(1, 1, 1);
    disc_ = GenMeshCylinder(1, 1, 48);
}

BankScene::~BankScene() {
    if (floor_.vertexCount) UnloadMesh(floor_);
    if (walls_.vertexCount) UnloadMesh(walls_);
    UnloadMesh(cube_);
    UnloadMesh(disc_);
    // The material owns both the bank shader and its visibility texture.
    UnloadMaterial(material_);
}

void BankScene::prepare(const TileMap& map, float wallHeight) {
    if (map_ == &map && wallHeight_ == wallHeight) return;
    Geometry floor, walls;
    const float size = static_cast<float>(map.tileSize());
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const float x0 = x * size, x1 = (x + 1) * size;
            const float z0 = y * size, z1 = (y + 1) * size;
            if (map.tile(x, y) != TileType::Wall) {
                floor.quad({x0, 0, z1}, {x1, 0, z1}, {x1, 0, z0}, {x0, 0, z0}, {0, 1, 0});
                // Visible ceiling detail is presentation only, never bullet cover.
                walls.quad({x0, wallHeight, z0}, {x1, wallHeight, z0}, {x1, wallHeight, z1},
                           {x0, wallHeight, z1}, {0, -1, 0});
                continue;
            }
            const auto exposed = [&](int tx, int ty) {
                return map.contains(tx, ty) && map.tile(tx, ty) != TileType::Wall;
            };
            if (exposed(x - 1, y))
                walls.quad({x0, 0, z0}, {x0, 0, z1}, {x0, wallHeight, z1}, {x0, wallHeight, z0},
                           {-1, 0, 0});
            if (exposed(x + 1, y))
                walls.quad({x1, 0, z1}, {x1, 0, z0}, {x1, wallHeight, z0}, {x1, wallHeight, z1},
                           {1, 0, 0});
            if (exposed(x, y - 1))
                walls.quad({x1, 0, z0}, {x0, 0, z0}, {x0, wallHeight, z0}, {x1, wallHeight, z0},
                           {0, 0, -1});
            if (exposed(x, y + 1))
                walls.quad({x0, 0, z1}, {x1, 0, z1}, {x1, wallHeight, z1}, {x0, wallHeight, z1},
                           {0, 0, 1});
            walls.quad({x0, wallHeight, z1}, {x1, wallHeight, z1}, {x1, wallHeight, z0},
                       {x0, wallHeight, z0}, {0, 1, 0});
        }
    auto newFloor = floor.upload();
    Mesh newWalls{};
    try {
        newWalls = walls.upload();
    } catch (...) {
        if (newFloor.vertexCount) UnloadMesh(newFloor);
        throw;
    }
    auto image = GenImageColor(map.width(), map.height(), WHITE);
    auto texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!texture.id) {
        if (newFloor.vertexCount) UnloadMesh(newFloor);
        if (newWalls.vertexCount) UnloadMesh(newWalls);
        throw std::runtime_error("Unable to allocate bank visibility texture");
    }
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    if (floor_.vertexCount) UnloadMesh(floor_);
    if (walls_.vertexCount) UnloadMesh(walls_);
    if (visibility_.id) UnloadTexture(visibility_);
    floor_ = newFloor;
    walls_ = newWalls;
    visibility_ = texture;
    material_.maps[MATERIAL_MAP_DIFFUSE].texture = visibility_;
    visibilityPixels_.resize(static_cast<std::size_t>(map.width()) * map.height());
    map_ = &map;
    wallHeight_ = wallHeight;
    ++geometryRevision_;
}

void BankScene::box(Vector3 center, Vector3 size, Color color, float mode) {
    SetShaderValue(material_.shader, modeLoc_, &mode, SHADER_UNIFORM_FLOAT);
    material_.maps[MATERIAL_MAP_DIFFUSE].color = color;
    DrawMesh(cube_, material_, meshTransform(size, center));
}

void BankScene::drawDoors(const TileMap& map, float height, Color accent) {
    const float size = static_cast<float>(map.tileSize());
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const auto tile = map.tile(x, y);
            if (!doorTile(tile)) continue;
            const auto center = map.tileCenter({x, y});
            const bool acrossX = map.tile(x - 1, y) == TileType::Wall ||
                                 map.tile(x + 1, y) == TileType::Wall ||
                                 tile == TileType::VaultDoor || tile == TileType::Gate ||
                                 tile == TileType::FrontDoor;
            const Vector3 across = acrossX ? Vector3{1, 0, 0} : Vector3{0, 0, 1};
            const auto trim = ColorLerp(accent, Palette::Bone, .18f);
            for (float sign : {-1.f, 1.f}) {
                box({center.x + across.x * sign * size * .46f, height * .5f,
                     center.y + across.z * sign * size * .46f},
                    acrossX ? Vector3{size * .08f, height, size * .22f}
                            : Vector3{size * .22f, height, size * .08f},
                    trim, 1);
            }
            box({center.x, height * .96f, center.y},
                acrossX ? Vector3{size, height * .08f, size * .22f}
                        : Vector3{size * .22f, height * .08f, size},
                trim, 1);
            if (!map.isOpen(x, y)) {
                box({center.x, height * .45f, center.y},
                    acrossX ? Vector3{size * .84f, height * .9f, size * .12f}
                            : Vector3{size * .12f, height * .9f, size * .84f},
                    ColorLerp(accent, Palette::Slate, .7f), 1);
                box({center.x + across.x * size * .23f, height * .43f,
                     center.y + across.z * size * .23f},
                    {size * .08f, size * .08f, size * .2f}, Palette::Bone, 1);
                if (tile == TileType::VaultDoor) {
                    material_.maps[MATERIAL_MAP_DIFFUSE].color = Palette::Bone;
                    const float radius = size * .3f;
                    const auto transform =
                        meshTransform({radius, size * .04f, radius},
                                      {center.x, height * .46f, center.y + size * .08f}, true);
                    DrawMesh(disc_, material_, transform);
                    box({center.x, height * .46f, center.y + size * .13f},
                        {size * .08f, size * .35f, size * .06f}, trim, 1);
                }
            }
        }
}

void BankScene::draw(const World& world, const RippleSystem& ripple, const ViewConfig& view,
                     const RenderConfig& render, float alpha, float phase,
                     const ObjectiveSystem* objectives) {
    const auto& map = world.level.map;
    prepare(map, view.wallHeight);
    const float size = static_cast<float>(map.tileSize());
    const auto playerPosition = world.player.interpolatedPosition(alpha);
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const float light = map.light(x, y) == LightLevel::Lit   ? 1.f
                                : map.light(x, y) == LightLevel::Dim ? .35f
                                                                     : 0.f;
            float reveal = std::max(light, ripple.tileReveal(x, y));
            float wall = reveal;
            if (map.tile(x, y) == TileType::Wall) {
                for (const auto offset :
                     {TileCoord{-1, 0}, TileCoord{1, 0}, TileCoord{0, -1}, TileCoord{0, 1}}) {
                    if (map.contains(x + offset.x, y + offset.y))
                        wall = std::max(wall, ripple.tileReveal(x + offset.x, y + offset.y));
                }
            }
            const auto center = map.tileCenter({x, y});
            const bool near =
                std::hypot(center.x - world.player.pos.x, center.y - world.player.pos.y) <=
                ripple.haloRadius() + size;
            const bool halo = near && Raycast::hasLineOfSight(world.player.pos, center, map);
            visibilityPixels_[static_cast<std::size_t>(y) * map.width() + x] = {
                channel(std::max(reveal, render.ambientFloorAlpha)),
                channel(std::max(wall, render.ambientWallAlpha)),
                static_cast<unsigned char>(halo ? 255 : 0), 255};
        }
    UpdateTexture(visibility_, visibilityPixels_.data());
    const float dimensions[2] = {map.width() * size, map.height() * size};
    const float player[2] = {playerPosition.x, playerPosition.y};
    const float halo = ripple.haloRadius();
    SetShaderValue(material_.shader, mapSizeLoc_, dimensions, SHADER_UNIFORM_VEC2);
    SetShaderValue(material_.shader, tileSizeLoc_, &size, SHADER_UNIFORM_FLOAT);
    SetShaderValue(material_.shader, phaseLoc_, &phase, SHADER_UNIFORM_FLOAT);
    SetShaderValue(material_.shader, playerLoc_, player, SHADER_UNIFORM_VEC2);
    SetShaderValue(material_.shader, haloLoc_, &halo, SHADER_UNIFORM_FLOAT);
    material_.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    float mode = 0;
    SetShaderValue(material_.shader, modeLoc_, &mode, SHADER_UNIFORM_FLOAT);
    if (floor_.vertexCount) DrawMesh(floor_, material_, meshTransform({1, 1, 1}, {}));
    mode = 1;
    SetShaderValue(material_.shader, modeLoc_, &mode, SHADER_UNIFORM_FLOAT);
    if (walls_.vertexCount) DrawMesh(walls_, material_, meshTransform({1, 1, 1}, {}));
    const Color accent = ColorLerp(Palette::Teal, Palette::Alarm, phase);
    drawDoors(map, view.wallHeight, accent);

    // Temporary readable 3D actors; detailed articulated art belongs to Step 5.
    const auto actor = [&](Vec2 position, float radius, float visibility, bool shield,
                           float facing) {
        if (visibility <= 0) return;
        const auto navy = Fade(Palette::Navy, visibility);
        box({position.x, 25, position.y}, {radius * 1.5f, 26, radius}, navy);
        box({position.x, 43, position.y}, {radius, 10, radius}, Fade(Palette::Ink, visibility));
        box({position.x + std::cos(facing) * radius * .45f, 42,
             position.y + std::sin(facing) * radius * .45f},
            {radius * .6f, 2, radius * .6f}, Fade(Palette::Bone, visibility));
        for (float sign : {-1.f, 1.f})
            box({position.x + sign * radius * .4f, 7, position.y}, {radius * .5f, 14, radius * .7f},
                navy);
        if (shield)
            box({position.x + std::cos(facing) * radius, 23,
                 position.y + std::sin(facing) * radius},
                {radius * 1.7f, 42, radius * .2f}, Fade(Palette::Slate, visibility));
    };
    const auto visible = [&](const Entity& entity) {
        if (world.alarmLoud) return entity.deathOpacity();
        return std::max(entity.reveal, ripple.visibility(static_cast<int>(entity.pos.x / size),
                                                         static_cast<int>(entity.pos.y / size),
                                                         world.player.pos, map)) *
               entity.deathOpacity();
    };
    for (const auto& guard : world.guards) {
        if (!guard.dead() && guard.state() != GuardState::Unconscious)
            actor(guard.interpolatedPosition(alpha), guard.radius, visible(guard), false,
                  guard.facing());
    }
    for (const auto& enemy : world.enemies) {
        if (!enemy->dead())
            actor(enemy->pos, enemy->radius, visible(*enemy), enemy->spec().id == "shield_cop",
                  enemy->facing());
    }
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const auto tile = map.tile(x, y);
            if (tile != TileType::Keycard && tile != TileType::SecurityPanel &&
                tile != TileType::Breaker && tile != TileType::BollardPanel)
                continue;
            const float reveal =
                world.alarmLoud ? 1.f : ripple.visibility(x, y, world.player.pos, map);
            if (reveal <= 0) continue;
            const auto position = map.tileCenter({x, y});
            box({position.x, 3, position.y}, {size * .23f, 6, size * .23f},
                Fade(tile == TileType::Keycard ? Palette::Alarm : Palette::Bone, reveal));
        }
    for (const auto& pickup : world.pickups) {
        const float reveal = visible(*pickup);
        if (reveal > 0)
            box({pickup->pos.x, 4, pickup->pos.y}, {8, 8, 8}, Fade(Palette::Bone, reveal));
    }
    for (const auto& camera : world.cameras) {
        const float reveal = visible(camera);
        if (reveal > 0)
            box({camera.pos.x, view.wallHeight * .75f, camera.pos.y},
                {size * .17f, size * .12f, size * .2f}, Fade(Palette::Bone, reveal));
    }
    for (const auto& laser : world.lasers) {
        const float reveal = visible(laser);
        if (reveal <= 0) continue;
        const auto end = laser.end();
        const float height = view.eyeHeight * .6f;
        const Color color =
            Fade(world.securityLoopRemaining > 0 ? Palette::Slate : Palette::Alarm, reveal);
        DrawLine3D({laser.pos.x, height, laser.pos.y}, {end.x, height, end.y}, color);
        box({laser.pos.x, height, laser.pos.y}, {3, 5, 3}, color);
        box({end.x, height, end.y}, {3, 5, 3}, color);
    }
    if (objectives)
        for (const auto& bag : objectives->bags()) {
            if (bag.state == BagState::Carried || bag.state == BagState::Delivered) continue;
            const float reveal = visible(bag);
            if (reveal > 0)
                box({bag.pos.x, 5, bag.pos.y}, {18, 10, 12}, Fade(Palette::Bone, reveal));
        }
    if (objectives && objectives->thermiteRemaining() > 0) {
        const auto position = objectives->vaultPosition();
        box({position.x, view.eyeHeight, position.y + size * .13f}, {6, 9, 4}, {242, 183, 5, 255});
    }
}
