#pragma once

#include <string>

#include "raylib.h"

class Logger;

// Window-owned font and logo cache. Construct after InitWindow, destroy before CloseWindow.
class UiAssets {
   public:
    explicit UiAssets(Logger& logger, const std::string& root = "assets");
    ~UiAssets();
    UiAssets(const UiAssets&) = delete;
    UiAssets& operator=(const UiAssets&) = delete;
    Font title() const { return title_; }
    Font button() const { return button_; }
    Font body() const { return body_; }
    Texture2D logo() const { return logo_; }
    bool fontsLoaded() const;
    void text(const char* value, Vector2 position, float size, Color color, bool body = false,
              bool bold = false) const;

   private:
    Font title_{};
    Font button_{};
    Font body_{};
    Texture2D logo_{};
};
