#include "core/Game.h"

#include <chrono>
#include <string>

#include "raylib.h"
#include "render/Renderer.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

Game::Game()
    : config_(Config::load("assets/config/tuning.json", logger_)),
      rng_(
          static_cast<std::uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())) {
    logger_.log(LogLevel::Info,
                "Ghost Protocol starting; RNG seed: " + std::to_string(rng_.seed()));
}

int Game::run() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Ghost Protocol");
    if (!IsWindowReady()) {
        logger_.log(LogLevel::Error, "Window initialization failed");
        return 1;
    }
    SetTargetFPS(60);
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* context) { static_cast<Game*>(context)->tick(); }, this,
                                 0, true);
#else
    while (!WindowShouldClose()) {
        tick();
    }
    CloseWindow();
    logger_.log(LogLevel::Info, "Ghost Protocol closed cleanly");
#endif
    return 0;
}

void Game::tick() {
    time_.addFrame(GetFrameTime());
    while (time_.consumeStep()) {
        update(static_cast<float>(Time::kStep));
    }
    Renderer::drawFoundation(time_.alpha());
}

void Game::update(float dt) { simulationSeconds_ += dt; }
