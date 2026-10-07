#pragma once
#include "render/Palette.h"
#include "render/Renderer.h"
#include "ui/ScreenNavigation.h"
inline void drawScreenButton(Renderer& renderer, const ScreenNavigation& navigation, int row,
                             float y, const char* label) {
    const auto b = ScreenNavigation::bounds(row, y);
    const float t = navigation.emphasis(row);
    DrawRectangleRounded({b.x, b.y, b.width, b.height}, .28f, 8, {20, 22, 27, 255});
    DrawRectangleRounded({b.x, b.y, b.width, b.height}, .28f, 8, Fade(Palette::Bone, t));
    DrawRectangleRoundedLinesEx({b.x, b.y, b.width, b.height}, .28f, 8, 2,
                                Fade(Palette::Bone, .2f));
    renderer.uiAssets().text(label, {b.x + 24, b.y + 15}, 24,
                             t > .5f ? Color{10, 10, 12, 255} : Palette::Bone);
}
