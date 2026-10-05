#pragma once

#include <istream>
#include <vector>

#include "core/Vec2.h"

enum class TileType : char {
    Wall = '#',
    Floor = '.',
    Door = 'd',
    ServiceDoor = 'S',
    CardDoor = 'R',
    Gate = 'G',
    VaultDoor = 'V',
    FrontDoor = 'F',
    Bollard = 'b',
    PickupZone = 'Z',
    Keycard = 'k',
    SecurityPanel = 'P',
    Breaker = 'B',
    BollardPanel = 'N',
    Money = 'M',
    PlayerSpawn = '@',
    VanSpawn = 'v'
};

enum class LightLevel { Dark, Dim, Lit };

struct TileCoord {
    int x = 0;
    int y = 0;
};

class TileMap {
   public:
    // Throws on malformed ASCII data. Asset callers handle the error at load time.
    static TileMap parse(std::istream& source, int tileSize);
    int width() const { return width_; }
    int height() const { return height_; }
    int tileSize() const { return tileSize_; }
    bool contains(int x, int y) const;
    TileType tile(int x, int y) const;
    bool isPassable(int x, int y) const;
    LightLevel light(int x, int y) const;
    void fillLight(LightLevel level);
    void setLight(int x, int y, LightLevel level);
    Vec2 tileCenter(TileCoord position) const;

   private:
    int width_ = 0;
    int height_ = 0;
    int tileSize_ = 0;
    std::vector<TileType> tiles_;
    std::vector<LightLevel> lights_;
};
