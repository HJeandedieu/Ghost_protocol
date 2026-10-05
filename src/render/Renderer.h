#pragma once

#include <cstdint>

#include "raylib.h"

struct Level;

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
    static void drawLevel(const Level& level, bool overview, std::uint32_t seed);

   private:
    RenderTexture2D surface_;
};
