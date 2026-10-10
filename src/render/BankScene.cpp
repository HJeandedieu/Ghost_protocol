#include "render/BankScene.h"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "core/Logger.h"
#include "core/ViewFootprint.h"
#include "entities/Enemy.h"
#include "entities/Guard.h"
#include "entities/Player.h"
#include "render/Palette.h"
#include "render/TacticalArt.h"
#include "render/VisibilityFan.h"
#include "systems/ObjectiveSystem.h"
#include "systems/RippleSystem.h"
#include "systems/VisionSystem.h"
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
    std::vector<unsigned char> colors;
    void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 normal, Color color = WHITE) {
        for (const auto p : {a, b, c, a, c, d}) {
            positions.insert(positions.end(), {p.x, p.y, p.z});
            normals.insert(normals.end(), {normal.x, normal.y, normal.z});
            colors.insert(colors.end(), {color.r, color.g, color.b, color.a});
        }
    }
    void box(Vector3 p, Vector3 size, Color color) {
        const float x = p.x - size.x * .5f, X = p.x + size.x * .5f;
        const float y = p.y - size.y * .5f, Y = p.y + size.y * .5f;
        const float z = p.z - size.z * .5f, Z = p.z + size.z * .5f;
        quad({x, y, z}, {x, y, Z}, {x, Y, Z}, {x, Y, z}, {-1, 0, 0}, color);
        quad({X, y, Z}, {X, y, z}, {X, Y, z}, {X, Y, Z}, {1, 0, 0}, color);
        quad({X, y, z}, {x, y, z}, {x, Y, z}, {X, Y, z}, {0, 0, -1}, color);
        quad({x, y, Z}, {X, y, Z}, {X, Y, Z}, {x, Y, Z}, {0, 0, 1}, color);
        quad({x, Y, Z}, {X, Y, Z}, {X, Y, z}, {x, Y, z}, {0, 1, 0}, color);
        quad({x, y, z}, {X, y, z}, {X, y, Z}, {x, y, Z}, {0, -1, 0}, color);
    }
    void cylinder(Vector3 p, float radius, float height, Color color) {
        constexpr int segments = 32;
        for (int i = 0; i < segments; ++i) {
            const float a = static_cast<float>(i) * 6.283185307f / segments;
            const float b = static_cast<float>(i + 1) * 6.283185307f / segments;
            const Vector3 A{p.x + std::cos(a) * radius, p.y, p.z + std::sin(a) * radius};
            const Vector3 B{p.x + std::cos(b) * radius, p.y, p.z + std::sin(b) * radius};
            const Vector3 C{B.x, p.y + height, B.z}, D{A.x, p.y + height, A.z};
            quad(A, D, C, B, {std::cos((a + b) * .5f), 0, std::sin((a + b) * .5f)}, color);
            quad({p.x, p.y + height, p.z}, C, D, D, {0, 1, 0}, color);
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
        mesh.colors =
            static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(colors.size())));
        if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.colors) {
            MemFree(mesh.vertices);
            MemFree(mesh.normals);
            MemFree(mesh.texcoords);
            MemFree(mesh.colors);
            throw std::runtime_error("Unable to allocate bank geometry");
        }
        std::memcpy(mesh.vertices, positions.data(), bytes);
        std::memcpy(mesh.normals, normals.data(), bytes);
        std::memcpy(mesh.colors, colors.data(), colors.size());
        std::memset(mesh.texcoords, 0,
                    static_cast<std::size_t>(mesh.vertexCount) * 2 * sizeof(float));
        UploadMesh(&mesh, false);
        return mesh;
    }
};

int interiorLastRow(const TileMap& map) {
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
            if (map.tile(x, y) == TileType::FrontDoor) return y;
    return map.height();
}
Geometry furnish(const TileMap& map, float height) {
    Geometry art;
    const float size = static_cast<float>(map.tileSize());
    const Color stone = ColorLerp(Palette::Slate, Palette::Bone, .48f);
    const Color wood = ColorLerp(Palette::Slate, Palette::Bone, .19f);
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            if (map.blocksSight(x, y)) continue;
            const auto center = map.tileCenter({x, y});
            const auto box = [&](float dx, float h, float dz, Vector3 extent, Color color) {
                art.box({center.x + dx * size, h, center.y + dz * size}, extent, color);
            };
            const auto shadow = [&](float rx, float rz) {
                art.box({center.x, .12f, center.y}, {rx * size, .08f, rz * size}, Palette::Ink);
            };
            for (const auto offset :
                 {TileCoord{-1, 0}, TileCoord{1, 0}, TileCoord{0, -1}, TileCoord{0, 1}}) {
                if (map.tile(x + offset.x, y + offset.y) != TileType::Wall) continue;
                const bool vertical = offset.x != 0;
                const Vector3 lower{vertical ? 2.f : size, 5, vertical ? size : 2.f};
                box(static_cast<float>(offset.x) * .49f, 2.5f, static_cast<float>(offset.y) * .49f,
                    lower, Palette::Slate);
                box(static_cast<float>(offset.x) * .49f, height - 4,
                    static_cast<float>(offset.y) * .49f,
                    {vertical ? 3.f : size, 3, vertical ? size : 3.f}, wood);
            }
            const bool counter = (x == 43 || x == 44 || x == 60 || x == 61) && y >= 23 && y <= 33;
            const bool desk = (x == 27 || x == 31) && (y == 22 || y == 25 || y == 30 || y == 34);
            const bool console = (x == 27 || x == 30 || x == 32) && y == 14;
            if (counter) {
                shadow(.98f, .98f);
                box(0, 11, 0, {size * .95f, 22, size}, stone);
                box(0, 23, 0, {size, 3, size}, Palette::Bone);
                for (float side : {-1.f, 1.f}) {
                    box(side * .477f, 10, 0, {1, 18, size * .88f}, wood);
                    box(side * .49f, 19, 0, {1, 1, size}, Palette::Slate);
                }
                if (y % 3 == 0) {
                    box(0, 26, 0, {size * .32f, 2, size * .22f}, Palette::Slate);
                    box(0, 31, -.1f, {size * .27f, 10, 2}, Palette::Ink);
                    box(0, 31, -.12f, {size * .23f, 7, .6f}, Palette::Teal);
                    box(.3f, 25, .14f, {size * .22f, 1, size * .16f}, Palette::Bone);
                    for (int key = 0; key < 6; ++key)
                        box(-.12f + static_cast<float>(key) * .047f, 27, .03f, {1, .4f, 2}, wood);
                }
            }
            if (desk || console) {
                shadow(1.5f, 1.2f);
                box(0, 21, 0, {size * 1.35f, 2.5f, size * .8f}, wood);
                for (float side : {-1.f, 1.f}) {
                    box(side * .54f, 10, 0, {size * .2f, 20, size * .7f}, Palette::Slate);
                    for (float drawer : {7.f, 14.f})
                        box(side * .54f, drawer, .36f, {size * .16f, 1, 1}, Palette::Bone);
                }
                box(0, 25, -.14f, {2, 6, 2}, Palette::Slate);
                box(0, 30, -.14f, {size * .38f, 14, 2}, Palette::Ink);
                box(0, 30, -.11f, {size * .33f, 10, .5f}, Palette::DeepTeal);
                for (int row = 0; row < 3; ++row)
                    box(-.04f, 28 + static_cast<float>(row) * 2, -.10f,
                        {size * (.22f - static_cast<float>(row) * .03f), .5f, .3f}, Palette::Teal);
                box(0, 23, .17f, {size * .34f, 1, size * .12f}, Palette::Slate);
                box(.43f, 23, .15f, {size * .23f, .6f, size * .17f}, Palette::Bone);
                box(0, 13, .66f, {size * .55f, 3, size * .42f}, Palette::Navy);
                box(0, 21, .88f, {size * .57f, 15, 3}, Palette::Slate);
                art.cylinder({center.x, .3f, center.y + size * .65f}, 2, 11, wood);
                box(0, 2, .66f, {size * .5f, 1, 2}, Palette::Slate);
                box(0, 2, .66f, {2, 1, size * .4f}, Palette::Slate);
            }
            if ((x == 44 || x == 57) && (y == 28 || y == 36)) {
                shadow(1.7f, .75f);
                box(0, 10, 0, {size * 1.65f, 3, size * .65f}, Palette::Slate);
                box(0, 17, -.29f, {size * 1.65f, 14, 3}, wood);
                for (float seat : {-.52f, 0.f, .52f})
                    box(seat, 13, 0, {size * .48f, 4, size * .55f}, Palette::Navy);
                for (float side : {-.74f, .74f}) box(side, 6, 0, {3, 12, size * .55f}, wood);
            }
            if ((x == 25 || x == 33) && (y == 18 || y == 26 || y == 36)) {
                shadow(.5f, .5f);
                art.cylinder({center.x, 0, center.y}, size * .17f, 10, wood);
                art.cylinder({center.x, 9, center.y}, size * .18f, 2, Palette::Slate);
                for (int leaf = 0; leaf < 9; ++leaf) {
                    const float angle = static_cast<float>(leaf) * 6.283185307f / 9;
                    const float dx = std::cos(angle), dz = std::sin(angle);
                    const Vector3 tip{center.x + dx * size * .38f,
                                      18 + static_cast<float>(leaf % 3) * 4,
                                      center.y + dz * size * .38f};
                    const Vector3 base{center.x, 10, center.y};
                    const Vector3 a{center.x + dx * size * .19f - dz * 3, 20,
                                    center.y + dz * size * .19f + dx * 3};
                    const Vector3 b{center.x + dx * size * .19f + dz * 3, 20,
                                    center.y + dz * size * .19f - dx * 3};
                    art.quad(base, a, tip, b, {0, 1, 0}, Palette::DeepTeal);
                    art.quad(base, b, tip, a, {0, -1, 0}, Palette::DeepTeal);
                }
            }
            if ((x == 6 || x == 10 || x == 14) && y == 34) {
                shadow(.95f, .95f);
                box(0, 10, 0, {size * .9f, 20, size * .9f}, wood);
                for (float side : {-.35f, .35f}) box(side, 10, .46f, {2, 20, 1}, Palette::Slate);
                box(0, 21, 0, {size * .94f, 2, size * .94f}, Palette::Slate);
            }
            // Coherent suspended lighting/ceiling beams derived from traversable floor tiles.
            if (y < interiorLastRow(map) && x % 4 == 0 && y % 4 == 0) {
                box(0, height - 2, 0, {size * .75f, 2, size * .24f}, Palette::Slate);
                box(0, height - 3.1f, 0, {size * .64f, .3f, size * .14f}, Palette::Bone);
            }
            if (map.tile(x, y) == TileType::Money) {
                box(0, 10, 0, {size * .8f, 20, size * .7f}, wood);
                for (float side : {-.18f, .18f})
                    box(side, 23, 0, {size * .27f, 5, size * .4f}, Palette::Bone);
            }
        }
    return art;
}

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
    if (furniture_.vertexCount) UnloadMesh(furniture_);
    UnloadMesh(cube_);
    UnloadMesh(disc_);
    // The material owns both the bank shader and its visibility texture.
    UnloadMaterial(material_);
}

void BankScene::prepare(const TileMap& map, float wallHeight) {
    if (map_ == &map && wallHeight_ == wallHeight) return;
    Geometry floor, walls;
    auto furniture = furnish(map, wallHeight);
    const float size = static_cast<float>(map.tileSize());
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const float x0 = x * size, x1 = (x + 1) * size;
            const float z0 = y * size, z1 = (y + 1) * size;
            if (map.tile(x, y) != TileType::Wall) {
                floor.quad({x0, 0, z1}, {x1, 0, z1}, {x1, 0, z0}, {x0, 0, z0}, {0, 1, 0});
                // Visible ceiling detail is presentation only, never bullet cover.
                if (y < interiorLastRow(map))
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
    Mesh newFurniture{}, newFloor{}, newWalls{};
    try {
        newFurniture = furniture.upload();
        newFloor = floor.upload();
        newWalls = walls.upload();
    } catch (...) {
        if (newFurniture.vertexCount) UnloadMesh(newFurniture);
        if (newFloor.vertexCount) UnloadMesh(newFloor);
        if (newWalls.vertexCount) UnloadMesh(newWalls);
        throw;
    }
    auto image = GenImageColor(map.width(), map.height(), WHITE);
    auto texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!texture.id) {
        if (newFurniture.vertexCount) UnloadMesh(newFurniture);
        if (newFloor.vertexCount) UnloadMesh(newFloor);
        if (newWalls.vertexCount) UnloadMesh(newWalls);
        throw std::runtime_error("Unable to allocate bank visibility texture");
    }
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    if (floor_.vertexCount) UnloadMesh(floor_);
    if (walls_.vertexCount) UnloadMesh(walls_);
    if (furniture_.vertexCount) UnloadMesh(furniture_);
    if (visibility_.id) UnloadTexture(visibility_);
    furniture_ = newFurniture;
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
                    acrossX ? Vector3{size * .08f, height, size}
                            : Vector3{size, height, size * .08f},
                    trim, 1);
            }
            box({center.x, height * .96f, center.y}, {size, height * .08f, size}, trim, 1);
            if (!map.isOpen(x, y)) {
                // Match the closed tile prism used by shooting, including its near face.
                box({center.x, height * .46f, center.y},
                    acrossX ? Vector3{size * .84f, height * .92f, size}
                            : Vector3{size, height * .92f, size * .84f},
                    ColorLerp(accent, Palette::Slate, .7f), 1);
                const Vector3 normal = acrossX ? Vector3{0, 0, 1} : Vector3{1, 0, 0};
                for (float sign : {-1.f, 1.f})
                    box({center.x + across.x * size * .23f + normal.x * sign * size * .52f,
                         height * .43f,
                         center.y + across.z * size * .23f + normal.z * sign * size * .52f},
                        acrossX ? Vector3{size * .08f, size * .08f, size * .12f}
                                : Vector3{size * .12f, size * .08f, size * .08f},
                        Palette::Bone, 1);
                if (tile == TileType::VaultDoor) {
                    material_.maps[MATERIAL_MAP_DIFFUSE].color = Palette::Bone;
                    const float radius = size * .3f;
                    const auto transform =
                        meshTransform({radius, size * .04f, radius},
                                      {center.x, height * .46f, center.y + size * .51f}, true);
                    DrawMesh(disc_, material_, transform);
                    box({center.x, height * .46f, center.y + size * .56f},
                        {size * .08f, size * .35f, size * .06f}, trim, 1);
                }
            }
        }
}

void BankScene::draw(const World& world, const RippleSystem& ripple, const ViewConfig& view,
                     const RenderConfig& render, float alpha, float phase,
                     const ObjectiveSystem* objectives, const ShotGeometryConfig& geometry,
                     TacticalArt* art, bool reduceEffects, const ViewFootprint* footprint) {
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
    mode = 3;
    SetShaderValue(material_.shader, modeLoc_, &mode, SHADER_UNIFORM_FLOAT);
    if (furniture_.vertexCount) DrawMesh(furniture_, material_, meshTransform({1, 1, 1}, {}));
    const Color accent = ColorLerp(Palette::Teal, Palette::Alarm, phase);
    drawDoors(map, view.wallHeight, accent);

    const auto visible = [&](const Entity& entity) {
        if (world.alarmLoud) return entity.deathOpacity();
        return std::max(entity.reveal, ripple.visibility(static_cast<int>(entity.pos.x / size),
                                                         static_cast<int>(entity.pos.y / size),
                                                         world.player.pos, map)) *
               entity.deathOpacity();
    };
    // Hazards use ping/proximity reveal even in lit rooms (Systems Contract 3.6).
    const auto hazardVisible = [&](const Entity& entity) {
        return (world.alarmLoud ? 1.f : entity.reveal) * entity.deathOpacity();
    };
    if (art) {
        art->beginActors();
        for (const auto& guard : world.guards) {
            if (visible(guard) > 0 &&
                (!footprint || footprint->intersects(guard.interpolatedPosition(alpha),
                                                     geometry.guardHeight * 1.2f)))
                art->drawActor(guard.id, guard.interpolatedPosition(alpha), guard.facing(),
                               guard.radius, geometry.guardHeight, TacticalKind::Guard,
                               visible(guard), phase, geometry,
                               guard.pos.x != guard.prevPos.x || guard.pos.y != guard.prevPos.y,
                               guard.dead() || guard.state() == GuardState::Unconscious);
        }
        for (const auto& enemy : world.enemies) {
            if (visible(*enemy) > 0 &&
                (!footprint || footprint->intersects(
                                   enemy->pos, geometry.enemyHeight(enemy->spec().id) * 1.2f))) {
                const auto kind = enemy->spec().id == "shield_cop" ? TacticalKind::Shield
                                  : enemy->spec().id == "heavy"    ? TacticalKind::Heavy
                                                                   : TacticalKind::Cop;
                art->drawActor(enemy->id, enemy->pos, enemy->facing(), enemy->radius,
                               geometry.enemyHeight(enemy->spec().id), kind, visible(*enemy), phase,
                               geometry,
                               enemy->pos.x != enemy->prevPos.x || enemy->pos.y != enemy->prevPos.y,
                               enemy->dead());
            }
        }
        art->endActors();
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
            box({position.x, tile == TileType::Keycard ? 3.f : 27.f, position.y},
                {size * .23f, tile == TileType::Keycard ? 6.f : 14.f, size * .23f},
                Fade(tile == TileType::Keycard ? Palette::Alarm : Palette::Bone, reveal));
        }
    for (const auto& pickup : world.pickups) {
        const float reveal = visible(*pickup);
        if (reveal > 0)
            box({pickup->pos.x, 4, pickup->pos.y}, {8, 8, 8}, Fade(Palette::Bone, reveal));
    }
    for (const auto& camera : world.cameras) {
        const float reveal = hazardVisible(camera);
        if (reveal > 0) {
            const float height = view.wallHeight * .75f;
            box({camera.pos.x, height, camera.pos.y}, {size * .22f, size * .14f, size * .28f},
                Fade(Palette::Bone, reveal));
            box({camera.pos.x, height + size * .09f, camera.pos.y}, {3, 5, 3},
                Fade(Palette::Slate, reveal));
            const Vector3 lens{
                camera.pos.x + std::cos(camera.facing() * .01745329252f) * size * .16f, height,
                camera.pos.y + std::sin(camera.facing() * .01745329252f) * size * .16f};
            DrawSphere(lens, 3, Fade(Palette::Ink, reveal));
            DrawSphere(
                {lens.x + std::cos(camera.facing() * .01745329252f) * 2, lens.y,
                 lens.z + std::sin(camera.facing() * .01745329252f) * 2},
                1.3f,
                Fade(world.securityLoopRemaining > 0 ? Palette::Slate : Palette::Alarm, reveal));
        }
    }
    for (const auto& laser : world.lasers) {
        const float reveal = hazardVisible(laser);
        if (reveal <= 0) continue;
        const auto end = laser.end();
        const float height = view.eyeHeight * .6f;
        const Color color =
            Fade(world.securityLoopRemaining > 0 ? Palette::Slate : Palette::Alarm, reveal);
        if (world.securityLoopRemaining <= 0) {
            const float length = std::hypot(end.x - laser.pos.x, end.y - laser.pos.y);
            const float distance = Raycast::sightDistance(laser.pos, end, map);
            const float fraction = length > 0 ? distance / length : 0;
            const Vector3 tip{laser.pos.x + (end.x - laser.pos.x) * fraction, height,
                              laser.pos.y + (end.y - laser.pos.y) * fraction};
            DrawCylinderEx({laser.pos.x, height, laser.pos.y}, tip, .5f, .5f, 8, color);
        }
        box({laser.pos.x, height, laser.pos.y}, {3, 5, 3}, color);
        box({end.x, height, end.y}, {3, 5, 3}, color);
    }
    if (objectives)
        for (const auto& bag : objectives->bags()) {
            if (bag.state == BagState::Carried || bag.state == BagState::Delivered) continue;
            const float reveal = visible(bag);
            if (reveal > 0) {
                box({bag.pos.x, 5, bag.pos.y}, {18, 10, 12}, Fade(Palette::Navy, reveal));
                for (float side : {-1.f, 1.f}) {
                    box({bag.pos.x + side * 5, 5, bag.pos.y}, {2, 10.5f, 12.5f},
                        Fade(Palette::Gold, reveal));
                    box({bag.pos.x + side * 3, 12, bag.pos.y}, {2, 5, 2},
                        Fade(Palette::Bone, reveal));
                }
                box({bag.pos.x, 14, bag.pos.y}, {8, 2, 2}, Fade(Palette::Bone, reveal));
            }
        }
    if (objectives && objectives->thermiteRemaining() > 0) {
        const auto position = objectives->vaultPosition();
        const Vector3 device{position.x, view.eyeHeight, position.y + size * .53f};
        box(device, {6, 9, 4}, Palette::Gold);
        const float age = objectives->thermiteAge();
        DrawSphere({device.x, device.y, device.z + 3},
                   2.5f + (reduceEffects ? 0.f : .5f * std::sin(age * 12.5663706f)),
                   Fade(Palette::Gold, .9f));
        for (int i = 0; !reduceEffects && i < 8; ++i) {
            const float life = std::fmod(age + static_cast<float>(i) / 12, .6666667f);
            const float angle = static_cast<float>(i) * 2.399963f;
            DrawLine3D({device.x, device.y, device.z + 3},
                       {device.x + std::cos(angle) * life * 20,
                        device.y + std::sin(angle) * life * 20 - life * life * 30,
                        device.z + 3 + life * 12},
                       Fade(Palette::Gold, 1 - life * 1.5f));
        }
    }
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            if (map.tile(x, y) == TileType::Bollard) {
                const auto point = map.tileCenter({x, y});
                const float height = objectives && objectives->bollardsLowered() ? 1.f : 20.f;
                box({point.x, height * .5f, point.y}, {7, height, 7}, Palette::Slate, 3);
                if (height > 1)
                    box({point.x, height * .75f, point.y}, {7.1f, 2, 7.1f}, Palette::Bone, 3);
            }
            if (map.tile(x, y) == TileType::VanSpawn && objectives && objectives->vanArrived()) {
                const auto point = map.tileCenter({x, y});
                box({point.x, 24, point.y}, {size * 2.8f, 34, size * 1.3f}, Palette::Slate, 3);
                box({point.x + size * .45f, 40, point.y}, {size * 1.8f, 5, size * 1.3f},
                    Palette::Navy, 3);
                for (float side : {-1.f, 1.f}) {
                    box({point.x + size * .65f, 34, point.y + side * size * .66f},
                        {size * .6f, 12, 1}, Palette::Ink, 3);
                    box({point.x - size * .65f, 26, point.y + side * size * .66f}, {1, 25, 1},
                        Palette::Bone, 3);
                    for (float wheel : {-1.f, 1.f}) {
                        Matrix transform{};
                        transform.m0 = 11;
                        transform.m6 = 11;
                        transform.m9 = -7;
                        transform.m12 = point.x + wheel * size * .95f;
                        transform.m13 = 11;
                        transform.m14 = point.y + side * size * .57f;
                        transform.m15 = 1;
                        material_.maps[MATERIAL_MAP_DIFFUSE].color = Palette::Ink;
                        DrawMesh(disc_, material_, transform);
                    }
                    box({point.x - size * 1.42f, 20, point.y + side * size * .4f}, {1, 5, 7},
                        Palette::Alarm, 3);
                }
            }
        }
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    if (!world.alarmLoud) {
        const auto triangle = [](VisibilityTriangle tri, Color color) {
            DrawTriangle3D({tri.a.x, .45f, tri.a.y}, {tri.b.x, .45f, tri.b.y},
                           {tri.c.x, .45f, tri.c.y}, color);
        };
        for (const auto& guard : world.guards) {
            if (guard.dead() || guard.state() == GuardState::Unconscious) continue;
            const float visibility = visible(guard);
            if (visibility <= 0) continue;
            const VisionSystem vision(guard.visionConfig());
            const std::array<float, 3> ranges{
                vision.rangeFor(LightLevel::Lit, world.player.isCrouched()),
                vision.rangeFor(LightLevel::Dim, world.player.isCrouched()),
                vision.rangeFor(LightLevel::Dark, world.player.isCrouched())};
            const auto position = guard.interpolatedPosition(alpha);
            const auto fan =
                visibilityFan(position, guard.facing(),
                              guard.visionConfig().coneDeg * .00872664626f, ranges, map);
            const bool alerted = guard.state() == GuardState::Alerted;
            for (auto tri : fan)
                triangle(tri, Fade(alerted ? Palette::Alarm : Palette::Bone,
                                   (alerted ? .25f : .18f) * visibility));
        }
        if (world.securityLoopRemaining <= 0)
            for (const auto& camera : world.cameras) {
                const float visibility = hazardVisible(camera);
                if (visibility <= 0) continue;
                const auto fan =
                    visibilityFan(camera.pos, camera.facing() * .01745329252f,
                                  camera.coneDegrees() * .00872664626f,
                                  {camera.range(), camera.range(), camera.range()}, map);
                for (auto tri : fan) triangle(tri, Fade(Palette::Alarm, .25f * visibility));
            }
        if (ripple.waveActive() && ripple.waveRadius() > 0) {
            const auto origin = ripple.origin();
            const auto boundary =
                visibilityBoundary(origin, 0, 3.14159265359f, ripple.waveRadius(), map);
            const float opacity = 1 - ripple.waveRadius() / ripple.maxRadius();
            for (std::size_t i = 1; i < boundary.size(); ++i) {
                triangle({origin, boundary[i], boundary[i - 1]},
                         Fade(Palette::Bone, .08f * opacity));
                DrawCylinderEx({boundary[i - 1].x, .6f, boundary[i - 1].y},
                               {boundary[i].x, .6f, boundary[i].y}, .6f, .6f, 6,
                               Fade(Palette::Bone, .9f * opacity));
            }
        }
    }

    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}
