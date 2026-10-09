#pragma once

#include <array>
#include <string>
#include <unordered_map>

#include "core/Config.h"
#include "core/Vec2.h"
#include "raylib.h"

class Logger;
class CombatSystem;

enum class TacticalKind { Guard, Cop, Shield, Heavy, Ghost };

// Original, cached procedural art. All coordinates below are visual-only proportions.
class TacticalArt {
   public:
    explicit TacticalArt(Logger& logger);
    ~TacticalArt();
    TacticalArt(const TacticalArt&) = delete;
    TacticalArt& operator=(const TacticalArt&) = delete;
    void resetMotion() { motion_.clear(); }
    void beginActors();
    void endActors();
    void drawActor(const std::string& id, Vec2 position, float facing, float radius, float height,
                   TacticalKind kind, float visibility, float phase,
                   const ShotGeometryConfig& geometry, bool moving);
    void drawForeground(const CombatSystem& combat, float phase);
    void drawGhostPortrait(Rectangle bounds);

   private:
    enum Part {
        Torso,
        HeavyTorso,
        Helmet,
        HeavyHelmet,
        Hood,
        UpperArm,
        Forearm,
        Hand,
        Thigh,
        Shin,
        Boot,
        Pistol,
        Smg,
        Shotgun,
        Shield,
        Count
    };
    std::array<Mesh, Count> meshes_{};
    Material material_{};
    RenderTexture2D portrait_{};
    int phaseLoc_ = -1, visibilityLoc_ = -1;
    struct Motion {
        Vec2 position;
        float travel = 0;
        bool seen = false;
    };
    std::unordered_map<std::string, Motion> motion_;
    void draw(Part part, Matrix transform);
    void lighting(float phase, float visibility);
};
