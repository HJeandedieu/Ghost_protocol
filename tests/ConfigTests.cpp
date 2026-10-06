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
    EXPECT_FLOAT_EQ(config.view.leadPx, 60.0f);
    EXPECT_FLOAT_EQ(config.view.followRate, 8.0f);
    EXPECT_FLOAT_EQ(config.ping.bigRadius, 520.0f);
    EXPECT_FLOAT_EQ(config.ping.hazardRevealRadius, 120.0f);
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

TEST(Config, HazardRevealRadiusLoadsOverrideAndRejectsMissingOrNegativeValues) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream console;
    Logger logger(console, "");
    data["ping"]["hazard_reveal_radius"] = 75;
    EXPECT_FLOAT_EQ(
        Config::load(files.write("override.json", data.dump()), logger).ping.hazardRevealRadius,
        75);
    data["ping"].erase("hazard_reveal_radius");
    EXPECT_FLOAT_EQ(
        Config::load(files.write("missing.json", data.dump()), logger).ping.hazardRevealRadius,
        120);
    data["ping"]["hazard_reveal_radius"] = -1;
    EXPECT_FLOAT_EQ(
        Config::load(files.write("invalid.json", data.dump()), logger).ping.hazardRevealRadius,
        120);
}

TEST(Config, HearingTuningLoadsOverridesAndMissingInvalidValuesFallBack) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream console;
    Logger logger(console, "");
    const auto shipped = Config::load("assets/config/tuning.json", logger);
    EXPECT_FLOAT_EQ(shipped.guard.searchLoopRadius, 96);
    EXPECT_FLOAT_EQ(shipped.guard.searchPointPause, 0.5f);
    EXPECT_FLOAT_EQ(shipped.guard.searchTurnRate, 90);
    EXPECT_FLOAT_EQ(shipped.guard.lookSweepDeg, 60);
    EXPECT_FLOAT_EQ(shipped.guard.stuckWindow, 1);
    EXPECT_FLOAT_EQ(shipped.guard.stuckMinProgress, 8);
    EXPECT_FLOAT_EQ(shipped.guard.arriveTolerance, 8);
    EXPECT_FLOAT_EQ(shipped.guard.pathClearStep, 12);
    EXPECT_FLOAT_EQ(shipped.guard.crumbSpacing, 32);
    EXPECT_FLOAT_EQ(shipped.guard.crumbMax, 64);
    data["guard"]["search_loop_radius"] = 72;
    data["guard"]["search_point_pause"] = 0.25;
    data["guard"]["path_clear_step"] = 6;
    data["guard"].erase("crumb_max");
    data["guard"]["stuck_window"] = -1;
    const auto custom = Config::load(files.write("hearing.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(custom.guard.searchLoopRadius, 72);
    EXPECT_FLOAT_EQ(custom.guard.searchPointPause, 0.25f);
    EXPECT_FLOAT_EQ(custom.guard.pathClearStep, 6);
    EXPECT_FLOAT_EQ(custom.guard.crumbMax, 64);
    EXPECT_FLOAT_EQ(custom.guard.stuckWindow, 1);
    EXPECT_NE(console.str().find("guard.crumb_max"), std::string::npos);
    EXPECT_NE(console.str().find("guard.stuck_window"), std::string::npos);
}

TEST(Config, CameraTuningOverridesAndMissingValuesUseDocumentedDefaults) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    data["view"]["lead_px"] = 40;
    data["view"].erase("follow_rate");
    std::ostringstream console;
    Logger logger(console, "");
    const auto config = Config::load(files.write("view.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(config.view.leadPx, 40);
    EXPECT_FLOAT_EQ(config.view.followRate, 8);
    EXPECT_NE(console.str().find("view.follow_rate"), std::string::npos);
}

TEST(Config, GuardRadiusAndStationaryTurnTuningLoadAndRejectInvalidValues) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    data["guard"]["radius"] = 12;
    data["guard"]["stationary_turn_speed"] = 15;
    std::ostringstream console;
    Logger logger(console, "");
    const auto custom = Config::load(files.write("guard.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(custom.guard.radius, 12);
    EXPECT_FLOAT_EQ(custom.guard.stationaryTurnSpeed, 15);
    data["guard"]["radius"] = -1;
    data["guard"].erase("stationary_turn_speed");
    const auto fallback = Config::load(files.write("guard-invalid.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(fallback.guard.radius, 14);
    EXPECT_FLOAT_EQ(fallback.guard.stationaryTurnSpeed, 20);
    EXPECT_NE(console.str().find("guard.radius"), std::string::npos);
    EXPECT_NE(console.str().find("guard.stationary_turn_speed"), std::string::npos);
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

TEST(Config, AmbientRenderAlphaLoadsOverridesAndRejectsOutOfRangeValues) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream console;
    Logger logger(console, "");
    data["render"] = {{"ambient_floor_alpha", 0.2}, {"ambient_wall_alpha", 0.4}};
    const auto valid = Config::load(files.write("valid.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(valid.render.ambientFloorAlpha, 0.2f);
    EXPECT_FLOAT_EQ(valid.render.ambientWallAlpha, 0.4f);
    data["render"] = {{"ambient_floor_alpha", -1}, {"ambient_wall_alpha", 1.1}};
    const auto invalid = Config::load(files.write("invalid.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(invalid.render.ambientFloorAlpha, 0.18f);
    EXPECT_FLOAT_EQ(invalid.render.ambientWallAlpha, 0.45f);
    data.erase("render");
    const auto missing = Config::load(files.write("missing.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(missing.render.ambientFloorAlpha, 0.18f);
    EXPECT_FLOAT_EQ(missing.render.ambientWallAlpha, 0.45f);
}
