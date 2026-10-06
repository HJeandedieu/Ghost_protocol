#include "render/Letterbox.h"

#include <algorithm>

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
