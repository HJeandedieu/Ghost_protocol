#pragma once

#include <cstdint>
#include <vector>

#include "core/Vec2.h"
#include "raylib.h"

struct Level;
class Player;
class Guard;
class RippleSystem;
class Logger;
class InteractionSystem;
class TileMap;
struct World;

class Renderer {
   public:
    explicit Renderer(Logger& logger);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void beginFrame();
    void present();
    void setReduceEffects(bool enabled) { reduceEffects_ = enabled; }
    Texture2D frameTexture() const { return surface_.texture; }
    void prepareLevel(const Level& level);
    void drawInteractionHud(const World& world, const InteractionSystem& interaction,
                            float noiseRadius, float maximumNoise) const;
    static void drawPlaceholder(const char* title, const char* subtitle);
    static void drawError(const char* message);
    void drawLevel(const Level& level, const Player& player, const RippleSystem& ripple,
                   Vec2 cameraTarget, float facing, float alpha, bool overview, std::uint32_t seed,
                   const std::vector<Guard>& guards = {});

   private:
    RenderTexture2D surface_;
    RenderTexture2D world_;
    Shader post_{};
    int timeLocation_ = -1;
    bool composed_ = false;
    bool reduceEffects_ = false;
    std::vector<float> coneAngles_;
    std::vector<float> coneDistances_;
    std::vector<Vec2> coneDirections_;
    struct ConeLightRegion {
        Rectangle bounds;
        float range;
    };
    std::vector<ConeLightRegion> coneLightRegions_;
    void drawGuardCone(const Guard& guard, Vec2 position, const TileMap& map, bool crouched,
                       float visibility);
    void compose(bool effects);
};
