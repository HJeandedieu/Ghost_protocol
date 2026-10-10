#include "render/TacticalArt.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "core/Logger.h"
#include "render/CharacterPose.h"
#include "render/Palette.h"
#include "rlgl.h"
#include "systems/CombatSystem.h"

namespace {
Vector3 add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 subtract(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 scale(Vector3 a, float amount) { return {a.x * amount, a.y * amount, a.z * amount}; }
Vector3 cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vector3 normalize(Vector3 a) {
    const float length = std::hypot(a.x, a.y, a.z);
    return length > 0 ? scale(a, 1 / length) : Vector3{};
}
Matrix identity() {
    Matrix m{};
    m.m0 = m.m5 = m.m10 = m.m15 = 1;
    return m;
}
Matrix scaling(float x, float y, float z) {
    auto m = identity();
    m.m0 = x;
    m.m5 = y;
    m.m10 = z;
    return m;
}
Matrix translation(float x, float y, float z) {
    auto m = identity();
    m.m12 = x;
    m.m13 = y;
    m.m14 = z;
    return m;
}
Matrix rotationX(float angle) {
    auto m = identity();
    m.m5 = m.m10 = std::cos(angle);
    m.m6 = std::sin(angle);
    m.m9 = -m.m6;
    return m;
}
Matrix rotationY(float angle) {
    auto m = identity();
    m.m0 = m.m10 = std::cos(angle);
    m.m8 = std::sin(angle);
    m.m2 = -m.m8;
    return m;
}
Matrix rotationZ(float angle) {
    auto m = identity();
    m.m0 = m.m5 = std::cos(angle);
    m.m1 = std::sin(angle);
    m.m4 = -m.m1;
    return m;
}
Matrix multiply(Matrix left, Matrix right) {
    const float a[] = {left.m0,  left.m1,  left.m2,  left.m3, left.m4,  left.m5,
                       left.m6,  left.m7,  left.m8,  left.m9, left.m10, left.m11,
                       left.m12, left.m13, left.m14, left.m15};
    const float b[] = {right.m0,  right.m1,  right.m2,  right.m3, right.m4,  right.m5,
                       right.m6,  right.m7,  right.m8,  right.m9, right.m10, right.m11,
                       right.m12, right.m13, right.m14, right.m15};
    float c[16]{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k) c[column * 4 + row] += b[k * 4 + row] * a[column * 4 + k];
    return {c[0], c[4], c[8],  c[12], c[1], c[5], c[9],  c[13],
            c[2], c[6], c[10], c[14], c[3], c[7], c[11], c[15]};
}

constexpr float kPi = 3.14159265358979323846f;
// Material families use the documented palette, with original tonal variations.
Color cloth() { return ColorLerp(Palette::Slate, Palette::Navy, .18f); }
Color armor() { return ColorLerp(Palette::Slate, Palette::Bone, .075f); }
Color seam() { return ColorLerp(Palette::Slate, Palette::Bone, .23f); }

struct Sculpt {
    std::vector<float> positions, normals;
    std::vector<unsigned char> colors;
    void vertex(Vector3 p, Vector3 n, Color color) {
        positions.insert(positions.end(), {p.x, p.y, p.z});
        normals.insert(normals.end(), {n.x, n.y, n.z});
        colors.insert(colors.end(), {color.r, color.g, color.b, color.a});
    }
    void triangle(Vector3 a, Vector3 b, Vector3 c, Color color) {
        const auto n = normalize(cross(subtract(b, a), subtract(c, a)));
        vertex(a, n, color);
        vertex(b, n, color);
        vertex(c, n, color);
    }
    void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) {
        triangle(a, b, c, color);
        triangle(a, c, d, color);
    }
    // Beveled armor/pouches, with eight-point profiles and chamfered top/bottom edges.
    void box(Vector3 p, Vector3 size, Color color, float bevel = .12f) {
        const float xs[] = {1, 1, 1 - bevel, -1 + bevel, -1, -1, -1 + bevel, 1 - bevel};
        const float zs[] = {-1 + bevel, 1 - bevel, 1, 1, 1 - bevel, -1 + bevel, -1, -1};
        const float ys[] = {-1, -1 + bevel, 1 - bevel, 1};
        const auto point = [&](int band, int side) {
            const float scale = band == 0 || band == 3 ? 1 - bevel : 1;
            return Vector3{p.x + xs[side] * size.x * .5f * scale, p.y + ys[band] * size.y * .5f,
                           p.z + zs[side] * size.z * .5f * scale};
        };
        for (int band = 0; band < 3; ++band)
            for (int side = 0; side < 8; ++side) {
                const int next = (side + 1) % 8;
                quad(point(band, side), point(band + 1, side), point(band + 1, next),
                     point(band, next), color);
            }
        for (int side = 0; side < 8; ++side) {
            const int next = (side + 1) % 8;
            triangle({p.x, p.y - size.y * .5f, p.z}, point(0, side), point(0, next), color);
            triangle({p.x, p.y + size.y * .5f, p.z}, point(3, next), point(3, side), color);
        }
    }
    void oval(Vector3 p, Vector3 radius, Color color) {
        constexpr int slices = 24, rings = 16;
        const auto point = [&](int row, int column) {
            const float latitude = kPi * static_cast<float>(row) / static_cast<float>(rings),
                        longitude =
                            2 * kPi * static_cast<float>(column) / static_cast<float>(slices);
            return Vector3{std::sin(latitude) * std::cos(longitude), std::cos(latitude),
                           std::sin(latitude) * std::sin(longitude)};
        };
        const auto add = [&](Vector3 unit) {
            vertex({p.x + unit.x * radius.x, p.y + unit.y * radius.y, p.z + unit.z * radius.z},
                   normalize({unit.x / radius.x, unit.y / radius.y, unit.z / radius.z}), color);
        };
        for (int row = 0; row < rings; ++row)
            for (int column = 0; column < slices; ++column) {
                const auto a = point(row, column), b = point(row + 1, column),
                           c = point(row + 1, column + 1), d = point(row, column + 1);
                add(a);
                add(c);
                add(b);
                add(a);
                add(d);
                add(c);
            }
    }
    void rod(Vector3 a, Vector3 b, float radius, Color color) {
        const auto axis = normalize(subtract(b, a));
        const auto across =
            normalize(cross(axis, std::fabs(axis.y) > .9f ? Vector3{1, 0, 0} : Vector3{0, 1, 0}));
        const auto up = cross(axis, across);
        constexpr int slices = 24;
        const auto normal = [&](int i) {
            return add(
                scale(across,
                      std::cos(2 * kPi * static_cast<float>(i) / static_cast<float>(slices))),
                scale(up, std::sin(2 * kPi * static_cast<float>(i) / static_cast<float>(slices))));
        };
        for (int i = 0; i < slices; ++i) {
            const auto n = normal(i), next = normal(i + 1);
            const auto a0 = add(a, scale(n, radius)), a1 = add(a, scale(next, radius));
            const auto b0 = add(b, scale(n, radius)), b1 = add(b, scale(next, radius));
            vertex(a0, n, color);
            vertex(b1, next, color);
            vertex(b0, n, color);
            vertex(a0, n, color);
            vertex(a1, next, color);
            vertex(b1, next, color);
            triangle(a, a0, a1, color);
            triangle(b, b1, b0, color);
        }
    }
    Mesh upload() const {
        Mesh mesh{};
        mesh.vertexCount = static_cast<int>(positions.size() / 3);
        mesh.triangleCount = mesh.vertexCount / 3;
        const auto floats = static_cast<unsigned int>(positions.size() * sizeof(float));
        const auto uvBytes = static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float));
        mesh.vertices = static_cast<float*>(MemAlloc(floats));
        mesh.normals = static_cast<float*>(MemAlloc(floats));
        mesh.texcoords = static_cast<float*>(MemAlloc(uvBytes));
        mesh.colors =
            static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(colors.size())));
        if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.colors) {
            MemFree(mesh.vertices);
            MemFree(mesh.normals);
            MemFree(mesh.texcoords);
            MemFree(mesh.colors);
            throw std::runtime_error("Unable to allocate tactical art");
        }
        std::memcpy(mesh.vertices, positions.data(), floats);
        std::memcpy(mesh.normals, normals.data(), floats);
        std::memcpy(mesh.colors, colors.data(), colors.size());
        std::memset(mesh.texcoords, 0, uvBytes);
        UploadMesh(&mesh, false);
        return mesh;
    }
};

Matrix local(Vector3 position, Vector3 scale = {1, 1, 1}, float angle = 0) {
    return multiply(multiply(scaling(scale.x, scale.y, scale.z), rotationX(angle)),
                    translation(position.x, position.y, position.z));
}

Matrix bone(Vector3 start, Vector3 end, float breadth = 1) {
    const auto displacement = subtract(end, start);
    const float length = std::hypot(displacement.x, displacement.y, displacement.z);
    const auto y = scale(normalize(displacement), -1);
    const auto x = normalize(cross(y, std::fabs(y.z) > .95f ? Vector3{0, 1, 0} : Vector3{0, 0, 1}));
    const auto z = cross(x, y);
    Matrix m{};
    m.m0 = x.x * breadth;
    m.m1 = x.y * breadth;
    m.m2 = x.z * breadth;
    m.m4 = y.x * length / .16f;
    m.m5 = y.y * length / .16f;
    m.m6 = y.z * length / .16f;
    m.m8 = z.x * breadth;
    m.m9 = z.y * breadth;
    m.m10 = z.z * breadth;
    m.m12 = start.x;
    m.m13 = start.y;
    m.m14 = start.z;
    m.m15 = 1;
    return m;
}

Sculpt torso(bool heavy) {
    Sculpt s;
    const float breadth = heavy ? 1.18f : 1.f;
    s.oval({0, .61f, 0}, {.145f * breadth, .16f, .082f}, cloth());
    s.box({0, .65f, .038f}, {.27f * breadth, .255f, .112f}, armor());
    s.box({0, .65f, .10f}, {.205f * breadth, .185f, .025f}, Palette::Slate);
    s.box({0, .47f, 0}, {.25f, .055f, .155f}, Palette::Ink);
    s.box({0, .47f, .085f}, {.038f, .033f, .015f}, seam());
    s.box({0, .75f, .113f}, {.08f, .025f, .009f}, Palette::Bone);
    s.box({0, .64f, -.091f}, {.22f, .2f, .035f}, armor());
    for (float sign : {-1.f, 1.f}) {
        s.box({sign * .102f, .7f, .101f}, {.036f, .16f, .018f}, Palette::Ink);
        s.box({sign * .108f, .724f, .116f}, {.027f, .035f, .013f}, seam());
        s.oval({sign * .17f * breadth, .73f, 0}, {.065f, .057f, .075f}, armor());
        s.box({sign * .168f * breadth, .735f, .083f}, {.06f, .035f, .018f}, Palette::Bone);
        s.box({sign * .09f, .53f, .104f}, {.068f, .08f, .05f}, cloth());
        s.box({sign * .09f, .555f, .133f}, {.062f, .024f, .008f}, armor());
        s.box({sign * .15f, .48f, 0}, {.04f, .08f, .09f}, armor());
        s.box({sign * .1f, .79f, 0}, {.027f, .08f, .115f}, Palette::Ink);
        for (int i = 0; i < 5; ++i)
            s.box({sign * .058f, .684f - static_cast<float>(i) * .025f, .123f},
                  {.057f, .009f, .007f}, seam(), .05f);
    }
    s.box({-.102f, .758f, .13f}, {.032f, .058f, .028f}, Palette::Ink);
    s.rod({-.10f, .782f, .13f}, {-.10f, .827f, .13f}, .004f, Palette::Ink);
    s.rod({.125f, .66f, -.106f}, {.08f, .795f, -.07f}, .007f, cloth());
    if (heavy) {
        s.box({0, .68f, .133f}, {.23f, .21f, .04f}, armor());
        for (float sign : {-1.f, 1.f})
            s.box({sign * .155f, .58f, .08f}, {.046f, .17f, .047f}, armor());
    }
    return s;
}

Sculpt head(bool ghost, bool heavy) {
    Sculpt s;
    s.rod({0, -.07f, 0}, {0, -.02f, 0}, .045f, Palette::Ink);
    s.oval({0, 0, 0}, {.092f, .112f, .091f}, Palette::Ink);
    s.oval({0, .04f, -.015f}, {ghost ? .111f : .107f, .098f, .104f}, ghost ? cloth() : armor());
    if (!ghost) {
        s.box({0, .012f, .089f}, {.164f, .022f, .027f}, Palette::Bone, .1f);
        s.box({0, -.025f, .095f}, {.143f, heavy ? .084f : .054f, .012f}, Palette::Ink, .18f);
        if (heavy) {
            s.box({0, -.025f, .105f}, {.11f, .07f, .012f}, armor());
            s.box({-.032f, -.025f, .114f}, {.009f, .066f, .009f}, Palette::Bone);
        }
        s.box({0, -.077f, .073f}, {.078f, .025f, .03f}, cloth());
    } else {
        // A curved original bone face shell, with angular recessed eye/nose cavities.
        const auto face = [](float x, float y, float offset = 0.f) {
            return Vector3{x, y, .099f + .022f * (1 - std::fabs(x) / .075f) + offset};
        };
        const std::vector<Vector3> outline = {
            face(-.075f, .015f),  face(-.058f, .037f),  face(0, .042f),      face(.058f, .037f),
            face(.075f, .015f),   face(.065f, -.032f),  face(.041f, -.058f), face(.025f, -.088f),
            face(-.025f, -.088f), face(-.041f, -.058f), face(-.065f, -.032f)};
        for (std::size_t i = 0; i < outline.size(); ++i)
            s.triangle(face(0, -.024f), outline[(i + 1) % outline.size()], outline[i],
                       Palette::Bone);
        for (float sign : {-1.f, 1.f}) {
            const auto a = face(sign * .062f, .016f, .002f), b = face(sign * .014f, .003f, .002f),
                       c = face(sign * .026f, -.025f, .002f), d = face(sign * .052f, -.022f, .002f);
            if (sign < 0)
                s.quad(a, d, c, b, Palette::Ink);
            else
                s.quad(a, b, c, d, Palette::Ink);
        }
        s.triangle(face(0, -.014f, .003f), face(-.014f, -.048f, .003f), face(.014f, -.048f, .003f),
                   Palette::Ink);
        for (int gap = -2; gap <= 2; ++gap) {
            const float x = static_cast<float>(gap) * .010f;
            s.quad(face(x - .001f, -.088f, .003f), face(x + .001f, -.088f, .003f),
                   face(x + .001f, -.064f, .003f), face(x - .001f, -.064f, .003f), Palette::Ink);
        }
    }
    for (float sign : {-1.f, 1.f}) {
        s.oval({sign * .104f, -.01f, -.008f}, {.025f, .044f, .038f}, Palette::Ink);
        s.box({sign * .124f, -.012f, -.008f}, {.012f, .064f, .054f}, armor());
        s.rod({sign * .103f, .015f, -.02f}, {sign * .08f, .11f, -.02f}, .009f, seam());
        s.box({sign * .101f, .057f, -.019f}, {.014f, .035f, .025f}, Palette::Bone);
    }
    s.rod({-.117f, -.04f, .015f}, {-.072f, -.078f, .105f}, .004f, seam());
    s.box({-.061f, -.079f, .107f}, {.03f, .012f, .013f}, Palette::Ink);
    return s;
}

Sculpt weapon(int type) {
    Sculpt s;
    const bool pistol = type == 0, shotgun = type == 2;
    const float length = pistol ? .19f : shotgun ? .43f : .32f;
    s.box({0, 0, 0}, {pistol ? .046f : .057f, .046f, length}, armor());
    s.box({0, -.026f, -.047f}, {.044f, .039f, .095f}, Palette::Ink);
    s.box({0, -.077f, -.04f}, {.039f, .091f, .047f}, cloth());
    s.box({0, -.032f, .014f}, {.009f, .021f, .039f}, seam());
    s.box({0, -.041f, -.008f}, {.052f, .011f, .07f}, Palette::Ink);
    if (!pistol) {
        s.box({0, -.077f, .033f}, {.033f, .104f, .054f}, armor());
        s.box({0, -.126f, .032f}, {.037f, .012f, .058f}, seam());
        s.box({0, .016f, -length * .57f}, {.061f, .056f, .103f}, armor());
        s.box({0, .014f, -length * .7f}, {.068f, .077f, .017f}, Palette::Ink);
        s.box({0, .038f, -.025f}, {.043f, .056f, .023f}, Palette::Ink);
        for (float sign : {-1.f, 1.f}) {
            s.box({sign * .019f, .077f, -.025f}, {.009f, .054f, .013f}, armor());
            s.box({sign * .029f, .005f, .105f}, {.011f, .04f, .135f}, Palette::Ink);
            for (int i = 0; i < 6; ++i)
                s.box({sign * .032f, .013f, .06f + static_cast<float>(i) * .022f},
                      {.012f, .013f, .014f}, seam());
        }
        s.box({0, .104f, -.025f}, {.048f, .009f, .015f}, armor());
        // Open optical window; the reticle remains screen-space and authoritative.
        for (int i = 0; i < 9; ++i)
            s.box({0, .027f, -.11f + static_cast<float>(i) * .035f}, {.044f, .01f, .012f}, seam());
        if (shotgun) {
            s.rod({0, -.018f, .085f}, {0, -.018f, .245f}, .014f, armor());
            for (int i = 0; i < 7; ++i)
                s.box({0, -.035f, .086f + static_cast<float>(i) * .019f}, {.062f, .028f, .011f},
                      Palette::Ink);
        }
    } else {
        s.box({0, .027f, -.078f}, {.046f, .017f, .021f}, Palette::Ink);
        s.box({0, .037f, -.078f}, {.024f, .006f, .008f}, Palette::Bone);
    }
    const float end = length * .5f;
    s.rod({0, .003f, end}, {0, .003f, end + (pistol ? .16f : .055f)}, pistol ? .021f : .014f,
          Palette::Ink);
    s.rod({0, .003f, end + (pistol ? .16f : .055f)}, {0, .003f, end + (pistol ? .163f : .058f)},
          pistol ? .012f : .009f, seam());
    s.rod({0, .003f, end + (pistol ? .164f : .059f)}, {0, .003f, end + (pistol ? .165f : .060f)},
          pistol ? .008f : .006f, Palette::Ink);
    s.box({.031f, -.002f, -.03f}, {.012f, .009f, .019f}, seam());
    s.box({-.029f, -.018f, -.046f}, {.008f, .012f, .027f}, seam());
    s.box({0, .024f, end - .007f}, {.009f, .025f, .018f}, Palette::Ink);
    return s;
}
}  // namespace

TacticalArt::TacticalArt(Logger& logger) {
#ifdef __EMSCRIPTEN__
    const char* root = "assets/shaders/glsl100/";
#else
    const char* root = "assets/shaders/glsl330/";
#endif
    material_ = LoadMaterialDefault();
    const std::string base(root);
    material_.shader = LoadShader((base + "tactical.vs").c_str(), (base + "tactical.fs").c_str());
    phaseLoc_ = GetShaderLocation(material_.shader, "phase");
    visibilityLoc_ = GetShaderLocation(material_.shader, "visibility");
    if (phaseLoc_ < 0 || visibilityLoc_ < 0) {
        UnloadMaterial(material_);
        logger.log(LogLevel::Error, "Tactical art shader unavailable");
        throw std::runtime_error("Tactical art shader unavailable");
    }
    const auto setPalette = [&](const char* name, Color color) {
        const float value[] = {color.r / 255.f, color.g / 255.f, color.b / 255.f};
        SetShaderValue(material_.shader, GetShaderLocation(material_.shader, name), value,
                       SHADER_UNIFORM_VEC3);
    };
    setPalette("tealRim", Palette::Teal);
    setPalette("redRim", Palette::Alarm);
    setPalette("goldRim", Palette::Gold);
    try {
        meshes_[Torso] = torso(false).upload();
        meshes_[HeavyTorso] = torso(true).upload();
        meshes_[Helmet] = head(false, false).upload();
        meshes_[HeavyHelmet] = head(false, true).upload();
        meshes_[Hood] = head(true, false).upload();
        Sculpt upper, fore, hand, thigh, shin, boot, shield;
        upper.oval({0, -.075f, 0}, {.05f, .087f, .053f}, cloth());
        upper.box({0, -.05f, -.045f}, {.07f, .063f, .016f}, armor());
        upper.box({0, -.123f, .033f}, {.047f, .04f, .025f}, armor());
        fore.oval({0, -.078f, 0}, {.045f, .087f, .043f}, cloth());
        fore.box({0, -.07f, .036f}, {.047f, .083f, .018f}, armor());
        fore.box({0, -.145f, 0}, {.095f, .025f, .075f}, Palette::Ink);
        for (int i = 0; i < 3; ++i)
            fore.box({0, -.037f - static_cast<float>(i) * .027f, -.04f}, {.054f, .009f, .009f},
                     seam());
        hand.oval({0, -.027f, .004f}, {.04f, .045f, .03f}, Palette::Ink);
        for (int i = 0; i < 4; ++i) {
            hand.oval({(static_cast<float>(i) - 1.5f) * .017f, -.053f, .025f}, {.009f, .017f, .01f},
                      cloth());
            hand.box({(static_cast<float>(i) - 1.5f) * .017f, -.034f, .035f}, {.013f, .021f, .006f},
                     Palette::Bone);
        }
        hand.oval({.038f, -.022f, .008f}, {.013f, .028f, .019f}, cloth());
        thigh.oval({0, -.10f, 0}, {.058f, .115f, .058f}, cloth());
        thigh.box({0, -.105f, -.047f}, {.081f, .106f, .03f}, armor());
        thigh.box({0, -.173f, .043f}, {.098f, .065f, .041f}, armor());
        thigh.box({0, -.171f, .065f}, {.055f, .03f, .016f}, seam());
        shin.oval({0, -.08f, 0}, {.043f, .092f, .044f}, cloth());
        shin.box({0, -.058f, .034f}, {.055f, .085f, .027f}, armor());
        boot.box({0, -.014f, .022f}, {.097f, .048f, .162f}, Palette::Ink);
        boot.box({0, -.033f, .027f}, {.103f, .015f, .174f}, seam());
        boot.box({0, .016f, -.015f}, {.075f, .057f, .083f}, armor());
        for (int i = 0; i < 4; ++i)
            boot.box({0, .022f, .014f + static_cast<float>(i) * .017f}, {.053f, .006f, .005f},
                     seam());
        shield.box({0, .5f, -.015f}, {1, 1, .03f}, Palette::Ink, .035f);
        shield.box({0, .5f, .004f}, {.95f, .95f, .01f}, armor(), .045f);
        shield.box({0, .5f, .012f}, {.87f, .88f, .007f}, Palette::Slate, .04f);
        shield.box({0, .78f, .019f}, {.7f, .125f, .007f}, Palette::Ink, .035f);
        shield.box({0, .78f, .024f}, {.62f, .065f, .006f}, Palette::Bone, .025f);
        for (float sign : {-1.f, 1.f}) {
            shield.box({sign * .462f, .5f, .02f}, {.024f, .9f, .015f}, Palette::Ink);
            for (float y : {.07f, .27f, .57f, .93f})
                shield.oval({sign * .426f, y, .024f}, {.012f, .009f, .004f}, seam());
        }
        shield.box({0, .35f, .017f}, {.9f, .008f, .008f}, Palette::Ink);
        meshes_[UpperArm] = upper.upload();
        meshes_[Forearm] = fore.upload();
        meshes_[Hand] = hand.upload();
        meshes_[Thigh] = thigh.upload();
        meshes_[Shin] = shin.upload();
        meshes_[Boot] = boot.upload();
        meshes_[Shield] = shield.upload();
        meshes_[Pistol] = weapon(0).upload();
        meshes_[Smg] = weapon(1).upload();
        meshes_[Shotgun] = weapon(2).upload();
        portrait_ = LoadRenderTexture(1024, 1024);
        if (!portrait_.id) throw std::runtime_error("Unable to allocate Ghost portrait");
        SetTextureFilter(portrait_.texture, TEXTURE_FILTER_BILINEAR);
        BeginTextureMode(portrait_);
        ClearBackground(Palette::Ink);
        Camera3D camera{{1.2f, .86f, 2.2f}, {0, .69f, 0}, {0, 1, 0}, 22, CAMERA_PERSPECTIVE};
        BeginMode3D(camera);
        lighting(.8f, 1);
        draw(Torso, identity());
        draw(Hood, local({0, .856f, 0}));
        for (float sign : {-1.f, 1.f}) {
            const Vector3 shoulder{sign * .17f, .73f, 0}, elbow{sign * .19f, .575f, .09f};
            const Vector3 wrist =
                sign < 0 ? Vector3{.002f, .60f, .32f} : Vector3{.068f, .561f, .135f};
            draw(UpperArm, bone(shoulder, elbow));
            draw(Forearm, bone(elbow, wrist));
            draw(Hand, local(wrist, {1, 1, 1}, sign < 0 ? -1.1f : 0));
        }
        draw(Smg, local({.04f, .612f, .18f}));
        EndMode3D();
        EndTextureMode();
    } catch (...) {
        for (auto mesh : meshes_)
            if (mesh.vertexCount) UnloadMesh(mesh);
        if (portrait_.id) UnloadRenderTexture(portrait_);
        UnloadMaterial(material_);
        throw;
    }
}

TacticalArt::~TacticalArt() {
    if (portrait_.id) UnloadRenderTexture(portrait_);
    for (auto mesh : meshes_)
        if (mesh.vertexCount) UnloadMesh(mesh);
    UnloadMaterial(material_);
}
void TacticalArt::lighting(float phase, float visibility) {
    SetShaderValue(material_.shader, phaseLoc_, &phase, SHADER_UNIFORM_FLOAT);
    SetShaderValue(material_.shader, visibilityLoc_, &visibility, SHADER_UNIFORM_FLOAT);
}
void TacticalArt::draw(Part part, Matrix transform) {
    DrawMesh(meshes_[part], material_, transform);
}

void TacticalArt::beginActors() {
    for (auto& entry : motion_) entry.second.seen = false;
}
void TacticalArt::endActors() {
    for (auto it = motion_.begin(); it != motion_.end();) {
        if (!it->second.seen)
            it = motion_.erase(it);
        else
            ++it;
    }
}

void TacticalArt::drawActor(const std::string& id, Vec2 position, float facing, float radius,
                            float height, TacticalKind kind, float visibility, float phase,
                            const ShotGeometryConfig& geometry, bool moving) {
    if (visibility <= 0 || height <= 0 || radius <= 0) return;
    lighting(phase, visibility);
    auto inserted = motion_.emplace(id, Motion{position, 0});
    auto& motion = inserted.first->second;
    motion.seen = true;
    const float distance =
        std::hypot(position.x - motion.position.x, position.y - motion.position.y);
    // Teleports/retries do not become an enormous walking stride.
    if (distance < height) motion.travel += distance;
    motion.position = position;
    const auto pose = CharacterPose::walking(motion.travel, height, moving);
    const float breadth = std::min(1.15f, radius / (height * .25f));
    const auto root =
        multiply(multiply(scaling(height * breadth, height, height), rotationY(kPi * .5f - facing)),
                 translation(position.x, 0, position.y));
    const auto place = [&](Part part, Matrix transform) { draw(part, multiply(transform, root)); };
    const bool heavy = kind == TacticalKind::Heavy, shield = kind == TacticalKind::Shield;
    place(heavy ? HeavyTorso : Torso, identity());
    place(kind == TacticalKind::Ghost ? Hood : heavy ? HeavyHelmet : Helmet, local({0, .856f, 0}));
    for (int side = 0; side < 2; ++side) {
        const float sign = side == 0 ? -1.f : 1.f;
        const float swing = side == 0 ? pose.leftLeg : pose.rightLeg;
        const float knee = side == 0 ? pose.leftKnee : pose.rightKnee;
        const auto hip = local({sign * .076f, .435f, 0}, {1, 1, 1}, swing);
        place(Thigh, hip);
        const auto lower = multiply(local({0, -.207f, 0}, {1, 1, 1}, -knee), hip);
        place(Shin, lower);
        place(Boot, multiply(local({0, -.184f, 0}), lower));
        const Vector3 shoulder{sign * (heavy ? .19f : .17f), .73f, 0};
        const Vector3 elbow{sign * .19f, .575f, .09f};
        const Vector3 wrist = side == 0 ? Vector3{.002f, .60f, .32f} : Vector3{.068f, .561f, .135f};
        place(UpperArm, bone(shoulder, elbow));
        place(Forearm, bone(elbow, wrist));
        place(Hand, local(wrist, {1, 1, 1}, side == 0 ? -1.1f : 0));
    }
    place(Smg, local({shield ? .1f : .04f, .612f, .18f},
                     heavy ? Vector3{1.15f, 1.15f, 1.15f} : Vector3{1, 1, 1}));
    if (shield) {
        // Model front is the exact configured gameplay plane. The plate thickness goes behind it.
        const auto plate = multiply(
            multiply(
                scaling(geometry.shieldWidth, geometry.shieldHeight, std::max(1.f, radius * .1f)),
                rotationY(kPi * .5f - facing)),
            translation(position.x + std::cos(facing) * (geometry.shieldForwardOffset -
                                                         std::max(1.f, radius * .1f) * .028f),
                        geometry.shieldBottom,
                        position.y + std::sin(facing) * (geometry.shieldForwardOffset -
                                                         std::max(1.f, radius * .1f) * .028f)));
        draw(Shield, plate);
    }
}

void TacticalArt::drawForeground(const CombatSystem& combat, float phase, bool reduceEffects) {
    // Dedicated viewmodel pass: +Z points forward, x right, y up. No bob or aim recoil.
    lighting(phase, 1);
    const auto& gun = combat.activeWeapon();
    const auto pose = WeaponPose::reload(gun.reloadRemaining(), gun.spec().reload);
    const bool pistol = gun.spec().id == "whisper", shotgun = gun.spec().id == "gavel";
    const auto root = multiply(multiply(rotationZ(pose.turn), rotationY(.23f)),
                               translation(-.24f, -.24f - pose.lower, .65f));
    draw(pistol ? Pistol : shotgun ? Shotgun : Smg, root);
    if (!reduceEffects && combat.shotAge() < .05f && !combat.lastShot().pellets.empty()) {
        const float barrel = pistol ? .258f : shotgun ? .273f : .218f;
        const Vector3 muzzle{root.m8 * barrel + root.m4 * .003f + root.m12,
                             root.m9 * barrel + root.m5 * .003f + root.m13,
                             root.m10 * barrel + root.m6 * .003f + root.m14};
        const float size = pistol ? .012f : .037f;
        DrawSphere(muzzle, size, Palette::Gold);
        for (float sign : {-1.f, 1.f})
            DrawTriangle3D({muzzle.x + sign * size * 2, muzzle.y, muzzle.z},
                           {muzzle.x, muzzle.y + size, muzzle.z},
                           {muzzle.x, muzzle.y - size, muzzle.z + size * 3}, Palette::Bone);
    }

    const Vector3 rightWrist{0, -.04f, -.04f};
    const Vector3 leftWrist = pistol ? Vector3{.04f, -.053f, -.025f} : Vector3{0, -.043f, .125f};
    draw(Forearm, multiply(bone({-.17f, -.42f, -.34f}, rightWrist, 1.8f), root));
    draw(Hand, multiply(local(rightWrist, {1.35f, 1.35f, 1.35f}), root));
    draw(Forearm, multiply(bone({.30f, -.42f, -.1f}, leftWrist, 1.8f), root));
    draw(Hand, multiply(local(leftWrist, {1.35f, 1.35f, 1.35f}, pistol ? 0 : -1.1f), root));
}

void TacticalArt::drawGhostPortrait(Rectangle bounds) {
    DrawTexturePro(portrait_.texture, {0, 0, 1024, -1024}, bounds, {0, 0}, 0, WHITE);
}
