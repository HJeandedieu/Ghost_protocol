#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/Logger.h"
#include "world/LevelLoader.h"

namespace {
nlohmann::json tinyLevel() {
    return {{"name", "Test Bank"},
            {"tile_size", 48},
            {"width", 5},
            {"height", 5},
            {"map_file", "test.map"},
            {"default_light", "dark"},
            {"guards",
             {{{"id", "G01"},
               {"type", "patrol_guard"},
               {"room", "Office"},
               {"mode", "stationary"},
               {"pager", true},
               {"facing", 180},
               {"wp", {{2, 1}}}}}},
            {"cameras",
             {{{"id", "C01"},
               {"room", "Office"},
               {"pos", {3, 1}},
               {"angle", 90},
               {"sweep", 25},
               {"range_px", 340}}}},
            {"lasers", {{{"id", "L01"}, {"a", {1, 2}}, {"b", {3, 2}}}}},
            {"light_zones", {{{"name", "Office"}, {"rect", {2, 1, 3, 2}}, {"level", "lit"}}}}};
}
const char* kTinyMap = "#####\n#@..#\n#...#\n#...#\n#####\n";
}  // namespace

TEST(LevelLoader, LoadsShippedBankAndDocumentedLightZoneBoundaries) {
    std::ostringstream console;
    Logger logger(console, "");
    const auto level = LevelLoader::load("assets/levels/gotham_central.json", logger);
    ASSERT_TRUE(level);
    EXPECT_EQ(level->map.width(), 80);
    EXPECT_EQ(level->map.height(), 56);
    EXPECT_EQ(level->map.tileSize(), 48);
    EXPECT_EQ(level->playerSpawn.x, 5);
    EXPECT_EQ(level->playerSpawn.y, 53);
    EXPECT_EQ(level->guards.size(), 11u);
    EXPECT_EQ(level->cameras.size(), 5u);
    EXPECT_EQ(level->lasers.size(), 3u);
    EXPECT_EQ(level->map.light(25, 12), LightLevel::Lit);
    EXPECT_EQ(level->map.light(33, 18), LightLevel::Lit);
    EXPECT_EQ(level->map.light(34, 18), LightLevel::Dark);
    EXPECT_EQ(level->map.light(40, 16), LightLevel::Dim);
    EXPECT_EQ(level->map.light(64, 40), LightLevel::Dim);
    EXPECT_EQ(level->map.light(5, 53), LightLevel::Dark);
    EXPECT_EQ(console.str().find("[ERROR]"), std::string::npos);
}

TEST(LevelLoader, LoadsTinyMapAndPreservesEntityMetadata) {
    TestFiles files;
    files.write("test.map", kTinyMap);
    std::ostringstream console;
    Logger logger(console, "");
    const auto level = LevelLoader::load(files.write("test.json", tinyLevel().dump()), logger);
    ASSERT_TRUE(level);
    EXPECT_EQ(level->map.width(), 5);
    EXPECT_EQ(level->guards[0].mode, PatrolMode::Stationary);
    EXPECT_TRUE(level->guards[0].pager);
    EXPECT_FLOAT_EQ(level->guards[0].facing, 180.0f);
    EXPECT_EQ(level->cameras[0].position.x, 3);
    EXPECT_FLOAT_EQ(level->cameras[0].range, 340.0f);
    EXPECT_EQ(level->lasers[0].b.x, 3);
    EXPECT_EQ(level->map.light(3, 2), LightLevel::Lit);
    EXPECT_EQ(level->map.light(1, 1), LightLevel::Dark);
}

TEST(LevelLoader, RejectsMissingMalformedAndMismatchedFilesWithoutThrowing) {
    TestFiles files;
    std::ostringstream console;
    Logger logger(console, "");
    EXPECT_FALSE(LevelLoader::load(files.path("missing.json"), logger));
    EXPECT_FALSE(LevelLoader::load(files.write("bad.json", "{bad"), logger));
    auto data = tinyLevel();
    EXPECT_FALSE(LevelLoader::load(files.write("test.json", data.dump()), logger));
    files.write("test.map", kTinyMap);
    data["width"] = 6;
    EXPECT_FALSE(LevelLoader::load(files.write("test.json", data.dump()), logger));
    EXPECT_NE(console.str().find("[ERROR]"), std::string::npos);
}

TEST(LevelLoader, RejectsInvalidGeometryLightsAndEntityContracts) {
    TestFiles files;
    files.write("test.map", kTinyMap);
    std::ostringstream console;
    Logger logger(console, "");
    auto rejects = [&](const nlohmann::json& data) {
        EXPECT_FALSE(LevelLoader::load(files.write("test.json", data.dump()), logger));
    };
    auto data = tinyLevel();
    data["guards"][0]["wp"] = {{0, 0}};
    rejects(data);
    data = tinyLevel();
    data["cameras"][0]["pos"] = {5, 1};
    rejects(data);
    data = tinyLevel();
    data["light_zones"][0]["rect"] = {3, 1, 2, 2};
    rejects(data);
    data = tinyLevel();
    data["light_zones"][0]["rect"] = {1, 1, 5, 2};
    rejects(data);
    data = tinyLevel();
    data["default_light"] = "bright";
    rejects(data);
    data = tinyLevel();
    data["guards"][0]["mode"] = "wander";
    rejects(data);
    data = tinyLevel();
    data["guards"][0]["wp"] = nlohmann::json::array();
    rejects(data);
    data = tinyLevel();
    data["cameras"][0]["id"] = "G01";
    rejects(data);
    data = tinyLevel();
    data["lasers"][0]["b"] = {3, 3};
    rejects(data);
    data = tinyLevel();
    data["tile_size"] = 48.5;
    rejects(data);
    data = tinyLevel();
    data["map_file"] = "../test.map";
    rejects(data);
    data = tinyLevel();
    data["guards"] = nullptr;
    rejects(data);
}

TEST(LevelLoader, RejectsMissingAndDuplicatePlayerSpawns) {
    TestFiles files;
    std::ostringstream console;
    Logger logger(console, "");
    const auto path = files.write("test.json", tinyLevel().dump());
    files.write("test.map", "#####\n#...#\n#...#\n#...#\n#####");
    EXPECT_FALSE(LevelLoader::load(path, logger));
    files.write("test.map", "#####\n#@@.#\n#...#\n#...#\n#####");
    EXPECT_FALSE(LevelLoader::load(path, logger));
}
