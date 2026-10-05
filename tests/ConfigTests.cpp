#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/Config.h"
#include "core/Logger.h"

TEST(Config, LoadsDocumentedTuningWithoutWarnings) {
    std::ostringstream console;
    Logger logger(console, "");
    const auto config = Config::load("assets/config/tuning.json", logger);
    EXPECT_FLOAT_EQ(config.player.walk, 160.0f);
    EXPECT_FLOAT_EQ(config.ping.bigRadius, 520.0f);
    EXPECT_FLOAT_EQ(config.mission.thermiteBurn, 75.0f);
    EXPECT_FLOAT_EQ(config.difficulty.hard.maxAlive, 16.0f);
    EXPECT_EQ(console.str().find("[WARN]"), std::string::npos);
}

TEST(Config, MissingKeyFallsBackAndPreservesOtherOverrides) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    data["player"].erase("walk");
    data["player"]["sprint"] = 275;
    std::ostringstream console;
    Logger logger(console, "");
    const auto config = Config::load(files.write("tuning.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(config.player.walk, 160.0f);
    EXPECT_FLOAT_EQ(config.player.sprint, 275.0f);
    EXPECT_NE(console.str().find("[WARN] Missing or invalid tuning key: player.walk"),
              std::string::npos);
}

TEST(Config, MissingFileAndMalformedJsonUseDefaults) {
    TestFiles files;
    std::ostringstream console;
    Logger logger(console, "");
    const auto missing = Config::load(files.path("missing.json"), logger);
    const auto malformed = Config::load(files.write("bad.json", "{broken"), logger);
    EXPECT_FLOAT_EQ(missing.player.walk, 160.0f);
    EXPECT_FLOAT_EQ(malformed.ping.cooldown, 3.0f);
    EXPECT_NE(console.str().find("Cannot open tuning file"), std::string::npos);
    EXPECT_NE(console.str().find("Invalid tuning file"), std::string::npos);
}

TEST(Config, WrongTypesAndNegativeNumbersDoNotReachGameState) {
    TestFiles files;
    std::ostringstream console;
    Logger logger(console, "");
    const auto config = Config::load(
        files.write("bad-values.json", R"({"player":{"walk":"fast","sprint":-5},"ping":false})"),
        logger);
    EXPECT_FLOAT_EQ(config.player.walk, 160.0f);
    EXPECT_FLOAT_EQ(config.player.sprint, 260.0f);
    EXPECT_FLOAT_EQ(config.ping.bigRadius, 520.0f);
    EXPECT_NE(console.str().find("player.sprint"), std::string::npos);
}
