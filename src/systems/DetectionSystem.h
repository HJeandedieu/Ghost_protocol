#pragma once

#include <vector>

class EventBus;
class Logger;
class Guard;
class Player;
class TileMap;

class DetectionSystem {
   public:
    DetectionSystem(EventBus& events, Logger& logger, std::vector<Guard>& guards,
                    float difficultyFill = 1.0f);
    void update(float dt, const Player& player, const TileMap& map, std::vector<Guard>& guards);

   private:
    EventBus& events_;
    Logger& logger_;
    const float difficultyFill_;
    void advanceCallIn(float dt, Guard& guard);
    void startCallIn(Guard& guard);
};
