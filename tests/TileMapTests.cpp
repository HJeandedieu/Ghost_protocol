#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

#include "world/TileMap.h"

TEST(TileMap, ParsesTinyMapAndConvertsTileCenters) {
    std::istringstream source("#####\r\n#@..#\r\n#.dS#\r\n#k..#\r\n#####\r\n");
    const auto map = TileMap::parse(source, 48);
    EXPECT_EQ(map.width(), 5);
    EXPECT_EQ(map.height(), 5);
    EXPECT_EQ(map.tile(1, 1), TileType::PlayerSpawn);
    EXPECT_TRUE(map.isPassable(2, 2));
    EXPECT_FALSE(map.isPassable(3, 2));
    EXPECT_FLOAT_EQ(map.tileCenter({1, 1}).x, 72.0f);
    EXPECT_FLOAT_EQ(map.tileCenter({1, 1}).y, 72.0f);
}

TEST(TileMap, DocumentedInitialPassabilityAndOutOfBoundsAreSafe) {
    const std::string symbols = "#.dSRGVFbZkPBNM@v";
    std::istringstream source(symbols);
    const auto map = TileMap::parse(source, 32);
    for (std::size_t x = 0; x < symbols.size(); ++x) {
        const bool blocked = std::string("#SRGVFb").find(symbols[x]) != std::string::npos;
        EXPECT_EQ(map.isPassable(static_cast<int>(x), 0), !blocked) << symbols[x];
    }
    EXPECT_FALSE(map.isPassable(-1, 0));
    EXPECT_FALSE(map.isPassable(map.width(), 0));
    EXPECT_FALSE(map.isPassable(0, map.height()));
    EXPECT_EQ(map.tile(-1, -1), TileType::Wall);
}

TEST(TileMap, StoresIndependentLightLevelsAndRejectsOutOfBoundsWrites) {
    std::istringstream source("..\n..");
    auto map = TileMap::parse(source, 48);
    EXPECT_EQ(map.light(0, 0), LightLevel::Dark);
    map.fillLight(LightLevel::Dim);
    map.setLight(1, 1, LightLevel::Lit);
    EXPECT_EQ(map.light(1, 1), LightLevel::Lit);
    EXPECT_EQ(map.light(0, 1), LightLevel::Dim);
    EXPECT_EQ(map.light(-1, 0), LightLevel::Dark);
    EXPECT_THROW(map.setLight(2, 0, LightLevel::Lit), std::out_of_range);
}

TEST(TileMap, RejectsEmptyRaggedUnknownAndInvalidSizeMaps) {
    for (const auto& text : {"", "##\n#", "##\n\n##", "#?"}) {
        std::istringstream source(text);
        EXPECT_THROW(TileMap::parse(source, 48), std::runtime_error);
    }
    std::istringstream source(".");
    EXPECT_THROW(TileMap::parse(source, 0), std::runtime_error);
}
