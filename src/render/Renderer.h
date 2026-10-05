#pragma once

#include <cstdint>

#include "core/Vec2.h"
#include "raylib.h"

struct Level;
class Player;
class RippleSystem;
class Logger;

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
    static void drawPlaceholder(const char* title, const char* subtitle);
    static void drawError(const char* message);
    void drawLevel(const Level& level, const Player& player, const RippleSystem& ripple,
                   Vec2 cameraTarget, float facing, float alpha, bool overview, std::uint32_t seed);

   private:
    RenderTexture2D surface_;
    RenderTexture2D world_;
    Shader post_{};
    int timeLocation_ = -1;
    bool composed_ = false;
    bool reduceEffects_ = false;
    void compose(bool effects);
};
