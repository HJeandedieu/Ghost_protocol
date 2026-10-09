#pragma once

#include <vector>

#include "core/Config.h"
#include "raylib.h"

class Logger;
class TileMap;
class RippleSystem;
class Player;
struct World;
class ObjectiveSystem;

// Window-owned cached architecture. It consumes world/reveal state without writing it.
class BankScene {
   public:
    explicit BankScene(Logger& logger);
    ~BankScene();
    BankScene(const BankScene&) = delete;
    BankScene& operator=(const BankScene&) = delete;
    void draw(const World& world, const RippleSystem& ripple, const ViewConfig& view,
              const RenderConfig& render, float alpha, float phase,
              const ObjectiveSystem* objectives);
    int geometryRevision() const { return geometryRevision_; }

   private:
    void prepare(const TileMap& map, float wallHeight);
    void box(Vector3 center, Vector3 size, Color color, float mode = 2);
    void drawDoors(const TileMap& map, float height, Color accent);
    Material material_{};
    Mesh floor_{}, walls_{}, cube_{}, disc_{};
    Texture2D visibility_{};
    const TileMap* map_ = nullptr;
    float wallHeight_ = 0;
    int geometryRevision_ = 0;
    int mapSizeLoc_ = -1, tileSizeLoc_ = -1, phaseLoc_ = -1;
    int modeLoc_ = -1, haloLoc_ = -1, playerLoc_ = -1;
    std::vector<Color> visibilityPixels_;
};
