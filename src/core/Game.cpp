#include "core/Game.h"

#include <chrono>
#include <string>

#include "raylib.h"
#include "render/Letterbox.h"
#include "render/Renderer.h"
#include "states/BootState.h"
#include "states/LoadoutState.h"
#include "states/MenuState.h"
#include "states/PayoutState.h"
#include "states/PlayState.h"
#include "world/LevelLoader.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

Game::Game()
    : config_(Config::load("assets/config/tuning.json", logger_)),
      saveStore_(logger_),
      settings_(saveStore_.loadSettings()),
      rng_(
          static_cast<std::uint32_t>(std::chrono::system_clock::now().time_since_epoch().count())) {
    logger_.log(LogLevel::Info,
                "Ghost Protocol starting; RNG seed: " + std::to_string(rng_.seed()));
}

Game::~Game() = default;

int Game::run() {
#ifdef __EMSCRIPTEN__
    SetConfigFlags(FLAG_MSAA_4X_HINT);
#else
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
#endif
    InitWindow(Letterbox::kWidth, Letterbox::kHeight, "Ghost Protocol");
    if (!IsWindowReady()) {
        logger_.log(LogLevel::Error, "Window initialization failed");
        return 1;
    }
#ifndef __EMSCRIPTEN__
    SetTargetFPS(60);
#endif
    SetExitKey(KEY_NULL);
    renderer_ = std::make_unique<Renderer>(logger_, config_.render);
    renderer_->setReduceEffects(settings_.reduceEffects);
    renderer_->setHints(settings_.hints);
#ifndef __EMSCRIPTEN__
    InitAudioDevice();
    SetMasterVolume(settings_.volumeMaster);
    if (settings_.fullscreen) toggleFullscreen();
#endif
#ifdef __EMSCRIPTEN__
    constexpr bool kWaitForClick = true;
#else
    constexpr bool kWaitForClick = false;
#endif
    states_.replace(std::make_unique<BootState>(input_, kWaitForClick, [this] {
#ifdef __EMSCRIPTEN__
        InitAudioDevice();
        SetMasterVolume(settings_.volumeMaster);
#endif
        showMenu();
    }));
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* context) { static_cast<Game*>(context)->tick(); }, this,
                                 0, true);
#else
    while (!quit_ && !WindowShouldClose()) {
        tick();
    }
    renderer_.reset();
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    logger_.log(LogLevel::Info, "Ghost Protocol closed cleanly");
#endif
    return 0;
}

void Game::tick() {
    // The key queue also retains short down/up taps occurring between rendered frames.
    for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
        if (key == KEY_ENTER || key == KEY_KP_ENTER) input_.confirmPressed = true;
        if (key == KEY_ESCAPE) input_.backPressed = true;
        if (key == KEY_DOWN || key == KEY_TAB) input_.menuVertical = 1;
        if (key == KEY_UP) input_.menuVertical = -1;
        if (key == KEY_LEFT) input_.menuHorizontal = -1;
        if (key == KEY_RIGHT) input_.menuHorizontal = 1;
        if (key == KEY_F11) {
            toggleFullscreen();
            settings_.fullscreen = IsWindowFullscreen();
            saveStore_.saveSettings(settings_);
        }
        if (key == KEY_C || key == KEY_LEFT_CONTROL) input_.crouchPressed = true;
        if (key == KEY_SPACE) input_.pingPressed = true;
        if (key == KEY_R) input_.reloadPressed = true;
        if (key == KEY_ONE) {
            input_.weaponSlot = 0;
            input_.loadoutExcluded = 0;
        }
        if (key == KEY_TWO) {
            input_.weaponSlot = 1;
            input_.loadoutExcluded = 1;
        }
        if (key == KEY_THREE) input_.loadoutExcluded = 2;
        if (key == KEY_E) input_.interactPressed = true;
        if (key == KEY_G) input_.throwPressed = true;
#ifndef NDEBUG
        if (key == KEY_F6) input_.debugDamagePressed = true;
        if (key == KEY_F7) input_.debugMedkitPressed = true;
        if (key == KEY_F9) input_.debugCopPressed = true;
        if (key == KEY_F8) input_.debugArmorPressed = true;
        if (key == KEY_F3) input_.debugPressed = true;
#endif
    }
    input_.firePressed = input_.firePressed || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input_.fireHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();
    if (wheel != 0) input_.weaponWheel += wheel > 0 ? 1 : -1;
    input_.startClicked = input_.startClicked || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input_.takedownPressed = input_.takedownPressed || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    input_.move = {static_cast<float>((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) -
                                      (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))),
                   static_cast<float>((IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) -
                                      (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)))};
    input_.sprintHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    input_.pingHeld = IsKeyDown(KEY_SPACE);
    input_.interactHeld = IsKeyDown(KEY_E);
    const auto viewport = Letterbox::fit(GetScreenWidth(), GetScreenHeight());
    const auto mouse = GetMousePosition();
    input_.mouseInViewport = viewport.width > 0 && viewport.height > 0 && mouse.x >= viewport.x &&
                             mouse.x < viewport.x + viewport.width && mouse.y >= viewport.y &&
                             mouse.y < viewport.y + viewport.height;
    if (input_.mouseInViewport) {
        input_.mouseLogical = {(mouse.x - viewport.x) * Letterbox::kWidth / viewport.width,
                               (mouse.y - viewport.y) * Letterbox::kHeight / viewport.height};
    }
    time_.addFrame(GetFrameTime());
    while (time_.consumeStep()) {
        update(static_cast<float>(Time::kStep));
    }
    renderer_->beginFrame();
    states_.render(time_.alpha());
    renderer_->present();
}

void Game::update(float dt) {
    renderer_->updateTransition(dt);
    states_.update(dt);
    input_.clearEdges();
}

void Game::showMenu(const std::string& error) {
    logger_.log(LogLevel::Info, "State: Menu");
    renderer_->startTransition(config_.ui.transitionTime, true);
    states_.replace(std::make_unique<MenuState>(
        input_, *renderer_, config_.ui, settings_, [this] { showLoadout(); },
        [this](const Settings& settings) { return applySettings(settings); },
        [this] { quit_ = true; }, error));
}

void Game::startMission(int stage, bool loud) {
    auto level = LevelLoader::load("assets/levels/gotham_central.json", logger_);
    if (!level) {
        showMenu("Unable to load the bank. Check the level files and try again.");
        return;
    }
    const auto weapons = loadWeapons("assets/config/weapons.json", logger_);
    if (!weapons) {
        showMenu("Unable to load weapons. Check the weapons file and try again.");
        return;
    }
    const auto enemies = loadEnemies("assets/config/enemies.json", logger_);
    if (!enemies) {
        showMenu("Unable to load enemies. Check the enemy file and try again.");
        return;
    }
    const auto waves = loadWaves("assets/config/enemies.json", logger_);
    const auto entries = loadSpawnPoints("assets/config/enemies.json", level->map, logger_);
    if (!waves || !entries) {
        showMenu("Unable to load police waves. Check the enemy file and try again.");
        return;
    }
    renderer_->startTransition(config_.ui.transitionTime);
    logger_.log(LogLevel::Info, "State: Play");
    states_.replace(std::make_unique<PlayState>(
        std::move(*level), input_, config_, rng_.seed(), *renderer_, logger_, *weapons, *enemies,
        *waves, *entries, stage, loud,
        [this](int retryStage, bool wasLoud) { startMission(retryStage, wasLoud); }, missionRun_,
        [this](Payout payout) { showPayout(std::move(payout)); }, loadout_, difficulty_));
}

bool Game::applySettings(const Settings& settings) {
    if (!saveStore_.saveSettings(settings)) return false;
    settings_ = settings;
    renderer_->setReduceEffects(settings_.reduceEffects);
    renderer_->setHints(settings_.hints);
    if (IsAudioDeviceReady()) SetMasterVolume(settings_.volumeMaster);
    if (IsWindowFullscreen() != settings_.fullscreen) toggleFullscreen();
    return true;
}

void Game::toggleFullscreen() {
#ifdef __EMSCRIPTEN__
    ToggleFullscreen();
#else
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
#endif
}

void Game::showLoadout() {
    renderer_->startTransition(config_.ui.transitionTime);
    states_.replace(std::make_unique<LoadoutState>(
        input_,
        [this](auto loadout, bool easy) {
            loadout_ = std::move(loadout);
            settings_.difficulty = easy ? "easy" : "normal";
            saveStore_.saveSettings(settings_);
            difficulty_ = easy ? config_.difficulty.easy : config_.difficulty.normal;
            if (!easy) difficulty_.maxAlive = config_.alarm.maxAlive;
            missionRun_ = std::make_shared<MissionRun>();
            startMission();
        },
        settings_.difficulty == "easy"));
}
void Game::showPayout(Payout payout) {
    renderer_->startTransition(config_.ui.transitionTime);
    logger_.log(LogLevel::Info, "State: Payout");
    states_.replace(
        std::make_unique<PayoutState>(input_, std::move(payout), [this] { showMenu(); }));
}
