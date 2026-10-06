#include "world/TileMap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

TileMap TileMap::parse(std::istream& source, int tileSize) {
    if (tileSize <= 0) throw std::runtime_error("Tile size must be positive");
    TileMap result;
    result.tileSize_ = tileSize;
    std::string row;
    while (std::getline(source, row)) {
        if (!row.empty() && row.back() == '\r') row.pop_back();
        if (row.empty() || row.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Empty or oversized map row");
        if (result.height_ == 0) result.width_ = static_cast<int>(row.size());
        if (static_cast<int>(row.size()) != result.width_)
            throw std::runtime_error("Map rows have different widths");
        for (char symbol : row) {
            if (std::string("#.dSRGVFbZkPBNM@v").find(symbol) == std::string::npos)
                throw std::runtime_error("Unknown map tile");
            result.tiles_.push_back(static_cast<TileType>(symbol));
        }
        ++result.height_;
    }
    if (source.bad() || result.height_ == 0) throw std::runtime_error("Unreadable or empty map");
    result.lights_.assign(result.tiles_.size(), LightLevel::Dark);
    result.open_.assign(result.tiles_.size(), false);
    return result;
}

bool TileMap::contains(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }

TileType TileMap::tile(int x, int y) const {
    if (!contains(x, y)) return TileType::Wall;
    return tiles_[static_cast<std::size_t>(y) * width_ + x];
}

bool TileMap::isPassable(int x, int y) const {
    if (isOpen(x, y)) return true;
    switch (tile(x, y)) {
        case TileType::Wall:
        case TileType::ServiceDoor:
        case TileType::CardDoor:
        case TileType::Gate:
        case TileType::VaultDoor:
        case TileType::FrontDoor:
        case TileType::Bollard:
            return false;
        default:
            return true;
    }
}

LightLevel TileMap::light(int x, int y) const {
    if (!contains(x, y)) return LightLevel::Dark;
    return lights_[static_cast<std::size_t>(y) * width_ + x];
}

bool TileMap::blocksSight(int x, int y) const {
    if (isOpen(x, y)) return false;
    switch (tile(x, y)) {
        case TileType::Wall:
        case TileType::Door:
        case TileType::ServiceDoor:
        case TileType::CardDoor:
        case TileType::Gate:
        case TileType::VaultDoor:
        case TileType::FrontDoor:
            return true;
        default:
            return false;
    }
}

bool TileMap::isOpen(int x, int y) const {
    return contains(x, y) && open_[static_cast<std::size_t>(y) * width_ + x];
}

void TileMap::setOpen(int x, int y, bool open) {
    if (!contains(x, y)) throw std::out_of_range("Door outside map");
    switch (tile(x, y)) {
        case TileType::Door:
        case TileType::ServiceDoor:
        case TileType::CardDoor:
        case TileType::Gate:
        case TileType::VaultDoor:
        case TileType::FrontDoor:
        case TileType::Bollard:
            open_[static_cast<std::size_t>(y) * width_ + x] = open;
            return;
        default:
            throw std::invalid_argument("Tile is not a door or bollard");
    }
}

void TileMap::removeKeycard(int x, int y) {
    if (tile(x, y) != TileType::Keycard) throw std::invalid_argument("Tile has no keycard");
    tiles_[static_cast<std::size_t>(y) * width_ + x] = TileType::Floor;
}

void TileMap::fillLight(LightLevel level) { std::fill(lights_.begin(), lights_.end(), level); }

void TileMap::setLight(int x, int y, LightLevel level) {
    if (!contains(x, y)) throw std::out_of_range("Light tile outside map");
    lights_[static_cast<std::size_t>(y) * width_ + x] = level;
}

Vec2 TileMap::tileCenter(TileCoord position) const {
    return {(static_cast<float>(position.x) + 0.5f) * tileSize_,
            (static_cast<float>(position.y) + 0.5f) * tileSize_};
}

Vec2 TileMap::moveCircle(Vec2 position, Vec2 displacement, float radius,
                         bool frontExitAllowed) const {
    if (tileSize_ <= 0 || radius <= 0.0f) return position;
    // Sweep one axis at a time. The circular extent at a tile edge preserves
    // corner clearance; sweeping the entire segment also prevents tunnelling.
    auto sweep = [&](bool horizontal, float delta) {
        if (delta == 0.0f) return;
        const float start = horizontal ? position.x : position.y;
        const float perpendicular = horizontal ? position.y : position.x;
        float finish = start + delta;
        const int axisLimit = horizontal ? width_ : height_;
        const int otherLimit = horizontal ? height_ : width_;
        const int first =
            std::clamp(static_cast<int>(std::floor((std::min(start, finish) - radius) / tileSize_)),
                       -1, axisLimit);
        const int last =
            std::clamp(static_cast<int>(std::floor((std::max(start, finish) + radius) / tileSize_)),
                       -1, axisLimit);
        const int otherFirst = std::clamp(
            static_cast<int>(std::floor((perpendicular - radius) / tileSize_)), -1, otherLimit);
        const int otherLast = std::clamp(
            static_cast<int>(std::floor((perpendicular + radius) / tileSize_)), -1, otherLimit);
        for (int a = first; a <= last; ++a) {
            for (int b = otherFirst; b <= otherLast; ++b) {
                const int tx = horizontal ? a : b, ty = horizontal ? b : a;
                if (isPassable(tx, ty) && (frontExitAllowed || tile(tx, ty) != TileType::FrontDoor))
                    continue;
                const float low = static_cast<float>(b) * tileSize_;
                const float separation =
                    perpendicular - std::clamp(perpendicular, low, low + tileSize_);
                if (separation * separation >= radius * radius) continue;
                const float extent = std::sqrt(radius * radius - separation * separation);
                const float near = static_cast<float>(a) * tileSize_ - extent;
                const float far = static_cast<float>(a + 1) * tileSize_ + extent;
                if (delta > 0.0f && start <= near) finish = std::min(finish, near);
                if (delta < 0.0f && start >= far) finish = std::max(finish, far);
            }
        }
        if (horizontal)
            position.x = finish;
        else
            position.y = finish;
    };
    sweep(true, displacement.x);
    sweep(false, displacement.y);
    return position;
}
