#pragma once

#include <vector>

#include "core/Config.h"

class EventBus;
class Logger;
class Guard;
class Player;
class TileMap;
class SecurityCamera;

class DetectionSystem {
   public:
    DetectionSystem(EventBus& events, Logger& logger, std::vector<Guard>& guards,
                    float difficultyFill = 1.0f);
    void update(float dt, const Player& player, const TileMap& map, std::vector<Guard>& guards);
    void bindCameras(std::vector<SecurityCamera>& cameras);
    void updateCameras(float dt, const Player& player, const TileMap& map,
                       std::vector<SecurityCamera>& cameras, const GuardConfig& fill,
                       bool disabled);

   private:
    EventBus& events_;
    Logger& logger_;
    const float difficultyFill_;
    void advanceCallIn(float dt, Guard& guard);
    void startCallIn(Guard& guard);
    void disableCameras(std::vector<SecurityCamera>& cameras);
};
