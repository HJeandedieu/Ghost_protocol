#include "ui/UiAssets.h"

#include <array>
#include <string>

#include "core/Logger.h"

namespace {
Font loadFont(const char* path, Logger& logger) {
    if (FileExists(path)) {
        std::array<int, 224> glyphs{};
        for (std::size_t i = 0; i < glyphs.size(); ++i) glyphs[i] = 32 + static_cast<int>(i);
        const auto font = LoadFontEx(path, 64, glyphs.data(), static_cast<int>(glyphs.size()));
        if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            return font;
        }
    }
    logger.log(LogLevel::Error, std::string("Font unavailable; using fallback: ") + path);
    return GetFontDefault();
}
void unloadFont(Font font) {
    if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
}
}  // namespace

UiAssets::UiAssets(Logger& logger, const std::string& root)
    : title_(loadFont((root + "/fonts/Orbitron-Bold.ttf").c_str(), logger)),
      button_(loadFont((root + "/fonts/Orbitron-Medium.ttf").c_str(), logger)),
      body_(loadFont((root + "/fonts/Inter-Medium.ttf").c_str(), logger)) {
    const auto portraitPath = root + "/ui/handler_portrait.png";
    if (FileExists(portraitPath.c_str())) handlerPortrait_ = LoadTexture(portraitPath.c_str());
    if (handlerPortrait_.id != 0)
        SetTextureFilter(handlerPortrait_, TEXTURE_FILTER_BILINEAR);
    else
        logger.log(LogLevel::Warn, "Handler portrait unavailable; using HUD fallback");
    const auto logoPath = root + "/ui/logo.png";
    const auto wordmarkPath = root + "/ui/logo_wordmark.png";
    if (FileExists(wordmarkPath.c_str())) wordmark_ = LoadTexture(wordmarkPath.c_str());
    if (wordmark_.id) SetTextureFilter(wordmark_, TEXTURE_FILTER_BILINEAR);
    if (FileExists(logoPath.c_str())) logo_ = LoadTexture(logoPath.c_str());
    if (logo_.id != 0)
        SetTextureFilter(logo_, TEXTURE_FILTER_BILINEAR);
    else
        logger.log(LogLevel::Error, "Logo unavailable; using text lockup");
}
UiAssets::~UiAssets() {
    unloadFont(title_);
    unloadFont(button_);
    unloadFont(body_);
    if (logo_.id != 0) UnloadTexture(logo_);
    if (wordmark_.id != 0) UnloadTexture(wordmark_);
    if (handlerPortrait_.id != 0) UnloadTexture(handlerPortrait_);
}
bool UiAssets::fontsLoaded() const {
    const auto fallback = GetFontDefault().texture.id;
    return title_.texture.id != fallback && button_.texture.id != fallback &&
           body_.texture.id != fallback;
}
void UiAssets::text(const char* value, Vector2 position, float size, Color color, bool body,
                    bool bold) const {
    DrawTextEx(body ? body_ : bold ? title_ : button_, value, position, size, 1, color);
}
