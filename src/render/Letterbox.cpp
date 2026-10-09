#include "render/Letterbox.h"

#include <algorithm>
#include <cmath>

Viewport Letterbox::fit(int windowWidth, int windowHeight) {
    if (windowWidth <= 0 || windowHeight <= 0) {
        return {};
    }
    const float scale = std::min(static_cast<float>(windowWidth) / kWidth,
                                 static_cast<float>(windowHeight) / kHeight);
    const float width = kWidth * scale;
    const float height = kHeight * scale;
    return {(windowWidth - width) * 0.5f, (windowHeight - height) * 0.5f, width, height};
}

RenderSize Letterbox::renderSize(int width, int height, int quality, int textureLimit) {
    const auto viewport = fit(width, height);
    const double units =
        std::ceil(std::max<double>(kWidth, viewport.width) / 16.0) * std::clamp(quality, 1, 4);
    const int limit = std::max(0, textureLimit / 16);
    const int supported = static_cast<int>(std::min(units, static_cast<double>(limit)));
    return {supported * 16, supported * 9, supported < units};
}
