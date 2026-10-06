#pragma once

struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

class Letterbox {
   public:
    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;
    static Viewport fit(int windowWidth, int windowHeight);
};
