#include "core/Game.h"

#include <chrono>
#include <string>

#include "raylib.h"
#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "states/BootState.h"
#include "states/MenuState.h"
#include "states/PlayState.h"
#include "world/LevelLoader.h"
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

Game::~Game() = default;

int Game::run() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(Letterbox::kWidth, Letterbox::kHeight, "Ghost Protocol");
    if (!IsWindowReady()) {
        logger_.log(LogLevel::Error, "Window initialization failed");
        return 1;
    }
    SetTargetFPS(60);
    renderer_ = std::make_unique<Renderer>();
#ifdef __EMSCRIPTEN__
    constexpr bool kWaitForClick = true;
#else
    constexpr bool kWaitForClick = false;
#endif
    states_.replace(std::make_unique<BootState>(input_, kWaitForClick, [this] { showMenu(); }));
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* context) { static_cast<Game*>(context)->tick(); }, this,
                                 0, true);
#else
    while (!WindowShouldClose()) {
        tick();
    }
    renderer_.reset();
    CloseWindow();
    logger_.log(LogLevel::Info, "Ghost Protocol closed cleanly");
#endif
    return 0;
}

void Game::tick() {
    // The key queue also retains short down/up taps occurring between rendered frames.
    for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
        if (key == KEY_ENTER || key == KEY_KP_ENTER) input_.confirmPressed = true;
        if (key == KEY_F11) toggleFullscreen();
#ifndef NDEBUG
        if (key == KEY_F3) input_.debugPressed = true;
#endif
    }
    input_.startClicked = input_.startClicked || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    time_.addFrame(GetFrameTime());
    while (time_.consumeStep()) {
        update(static_cast<float>(Time::kStep));
    }
    renderer_->beginFrame();
    states_.render(time_.alpha());
    renderer_->present();
}

void Game::update(float dt) {
    states_.update(dt);
    input_.clearEdges();
}

void Game::showMenu(const std::string& error) {
    logger_.log(LogLevel::Info, "State: Menu");
    states_.replace(std::make_unique<MenuState>(
        input_,
        [this] {
            auto level = LevelLoader::load("assets/levels/gotham_central.json", logger_);
            if (!level) {
                showMenu("Unable to load the bank. Check the level files and try again.");
                return;
            }
            logger_.log(LogLevel::Info, "State: Play");
            states_.replace(std::make_unique<PlayState>(std::move(*level), input_, rng_.seed()));
        },
        error));
}

void Game::toggleFullscreen() {
    if (!IsWindowFullscreen()) {
        windowedWidth_ = GetScreenWidth();
        windowedHeight_ = GetScreenHeight();
        const int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        ToggleFullscreen();
    } else {
        ToggleFullscreen();
        SetWindowSize(windowedWidth_, windowedHeight_);
    }
}
