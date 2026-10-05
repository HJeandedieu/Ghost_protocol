#pragma once

#include <cstdint>

#include "core/Vec2.h"
#include "raylib.h"

struct Level;
class Player;

class Renderer {
   public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void beginFrame() const;
    void present() const;
    static void drawPlaceholder(const char* title, const char* subtitle);
    static void drawError(const char* message);
    static void drawLevel(const Level& level, const Player& player, Vec2 cameraTarget, float facing,
                          float alpha, bool overview, std::uint32_t seed);

   private:
    RenderTexture2D surface_;
};
