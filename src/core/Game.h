#pragma once

#include "core/Config.h"
#include "core/Logger.h"
#include "core/Rng.h"
#include "core/Time.h"

class Game {
   public:
    Game();
    int run();

   private:
    void tick();
    void update(float dt);
    Logger logger_;
    const Config config_;
    Rng rng_;
    Time time_;
    double simulationSeconds_ = 0.0;
};
