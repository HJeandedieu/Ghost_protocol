#pragma once

#include "raylib.h"

class Renderer {
   public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void beginFrame() const;
    void present() const;
    static void drawPlaceholder(const char* title, const char* subtitle);

   private:
    RenderTexture2D surface_;
};
