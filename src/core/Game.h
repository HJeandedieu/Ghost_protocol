#pragma once

#include <memory>

#include "core/Config.h"
#include "core/Input.h"
#include "core/Logger.h"
#include "core/Rng.h"
#include "core/SaveStore.h"
#include "core/Time.h"
#include "states/StateMachine.h"
#include "systems/ScoreSystem.h"

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
    bool applySettings(const Settings& settings);
    void showLoadout();
    void showPayout(Payout payout);
    void startMission(int stage = 1, bool loud = false);
    Logger logger_;
    const Config config_;
    SaveStore saveStore_;
    Settings settings_;
    bool quit_ = false;
    Rng rng_;
    Time time_;
    Input input_;
    StateMachine states_;
    std::unique_ptr<Renderer> renderer_;
    std::array<std::string, 2> loadout_{{"whisper", "chatter"}};
    DifficultyPreset difficulty_;
    std::shared_ptr<MissionRun> missionRun_;

#ifndef __EMSCRIPTEN__
    int windowedWidth_ = 1280;
    int windowedHeight_ = 720;
#endif
};
