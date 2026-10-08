#pragma once

struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct RenderSize {
    int width = 0, height = 0;
    bool reduced = false;
};

class Letterbox {
   public:
    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;
    static Viewport fit(int windowWidth, int windowHeight);
    static RenderSize renderSize(int width, int height, int quality, int textureLimit);
};
