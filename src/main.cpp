#include <iostream>

#include "core/Assets.h"
#include "core/Game.h"
#include "raylib.h"

int main() {
#ifndef __EMSCRIPTEN__
    if (!Assets::useApplicationDirectory(GetApplicationDirectory())) {
        std::cerr << "[ERROR] Cannot enter the application directory\n";
        return 1;
    }
#endif
    Game game;
    return game.run();
}
