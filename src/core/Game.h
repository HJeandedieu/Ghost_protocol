#pragma once

#include <memory>

#include "core/Config.h"
#include "core/Input.h"
#include "core/Logger.h"
#include "core/Rng.h"
#include "core/Time.h"
#include "states/StateMachine.h"

class Renderer;

class Game {
   public:
    Game();
    ~Game();
    int run();

   private:
    void tick();
    void update(float dt);
    void showMenu(const std::string& error = "");
    void toggleFullscreen();
    void startMission(int stage = 1, bool loud = false);
    Logger logger_;
    const Config config_;
    Rng rng_;
    Time time_;
    Input input_;
    StateMachine states_;
    std::unique_ptr<Renderer> renderer_;
    int windowedWidth_ = 1280;
    int windowedHeight_ = 720;
};
