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
    EXPECT_EQ(config.mission.retryPositions[0].x, 21);
    EXPECT_EQ(config.mission.retryPositions[3].y, 8);
    EXPECT_FLOAT_EQ(config.difficulty.hard.maxAlive, 16.0f);
    EXPECT_EQ(console.str().find("[WARN]"), std::string::npos);
}

TEST(Config, RetryPositionsLoadOverridesAndRejectMalformedOrOverflowCoordinates) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    data["mission"]["retry_positions"] = {
        {"s3", {2, 3}}, {"s4", {-1, 4}}, {"s5", {1.5, 8}}, {"s6", {999999999999LL, 8}}};
    std::ostringstream console;
    Logger logger(console, "");
    const auto config = Config::load(files.write("retry.json", data.dump()), logger);
    EXPECT_EQ(config.mission.retryPositions[0].x, 2);
    EXPECT_EQ(config.mission.retryPositions[0].y, 3);
    EXPECT_EQ(config.mission.retryPositions[1].x, 52);
    EXPECT_EQ(config.mission.retryPositions[2].x, 52);
    EXPECT_EQ(config.mission.retryPositions[3].x, 52);
    EXPECT_NE(console.str().find("mission.retry_positions.s4"), std::string::npos);
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

TEST(Config, PickupTuningLoadsOverridesAndDefaults) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream console;
    Logger logger(console, "");
    const auto shipped = Config::load("assets/config/tuning.json", logger).pickup;
    EXPECT_FLOAT_EQ(shipped.medkitChance, 0.2f);
    EXPECT_FLOAT_EQ(shipped.armorChance, 0.1f);
    EXPECT_FLOAT_EQ(shipped.medkitAmount, 50);
    EXPECT_FLOAT_EQ(shipped.armorAmount, 50);
    EXPECT_FLOAT_EQ(shipped.collectRadius, 50);
    data["pickup"] = {{"medkit_chance", 0.4},
                      {"armor_chance", 0.3},
                      {"medkit_amount", 25},
                      {"armor_amount", 15},
                      {"collect_radius", 30}};
    const auto custom = Config::load(files.write("pickup.json", data.dump()), logger).pickup;
    EXPECT_FLOAT_EQ(custom.medkitChance, 0.4f);
    EXPECT_FLOAT_EQ(custom.armorChance, 0.3f);
    EXPECT_FLOAT_EQ(custom.medkitAmount, 25);
    EXPECT_FLOAT_EQ(custom.armorAmount, 15);
    EXPECT_FLOAT_EQ(custom.collectRadius, 30);
}

TEST(Config, PickupTuningRejectsProbabilitySumAndInvalidMissingKeys) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream console;
    Logger logger(console, "");
    data["pickup"] = {{"medkit_chance", 0.8},
                      {"armor_chance", 0.5},
                      {"medkit_amount", -1},
                      {"collect_radius", "bad"}};
    const auto invalid =
        Config::load(files.write("invalid-pickup.json", data.dump()), logger).pickup;
    EXPECT_FLOAT_EQ(invalid.medkitChance, 0.2f);
    EXPECT_FLOAT_EQ(invalid.armorChance, 0.1f);
    EXPECT_FLOAT_EQ(invalid.medkitAmount, 50);
    EXPECT_FLOAT_EQ(invalid.armorAmount, 50);
    EXPECT_FLOAT_EQ(invalid.collectRadius, 50);
    EXPECT_NE(console.str().find("Invalid pickup probability sum"), std::string::npos);
    data["pickup"]["medkit_chance"] = 2;
    data["pickup"]["armor_chance"] = -0.1;
    const auto badProbabilities =
        Config::load(files.write("bad-probabilities.json", data.dump()), logger).pickup;
    EXPECT_FLOAT_EQ(badProbabilities.medkitChance, 0.2f);
    EXPECT_FLOAT_EQ(badProbabilities.armorChance, 0.1f);
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

TEST(Config, PayoutDeductionLimitsRejectFractionalNegativeAndOverflow) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream output;
    Logger logger(output, "");
    data["payout"]["deduction_max_count"] = 1.5;
    data["payout"]["deduction_max_amount"] = -2;
    auto config = Config::load(files.write("payout-invalid.json", data.dump()), logger);
    EXPECT_EQ(config.payout.deductionMaxCount, 3);
    EXPECT_EQ(config.payout.deductionMaxAmount, 500);
    EXPECT_NE(output.str().find("payout.deduction_max_count"), std::string::npos);
    data["payout"]["deduction_max_count"] = 0;
    data["payout"]["deduction_max_amount"] = 999999999999LL;
    config = Config::load(files.write("payout-overflow.json", data.dump()), logger);
    EXPECT_EQ(config.payout.deductionMaxCount, 0);
    EXPECT_EQ(config.payout.deductionMaxAmount, 500);
}

TEST(Config, PayoutRatesRetainJsonDoublePrecision) {
    std::ostringstream output;
    Logger logger(output, "");
    const auto config = Config::load("assets/config/tuning.json", logger);
    EXPECT_DOUBLE_EQ(config.payout.handlerCut, .15);
    EXPECT_DOUBLE_EQ(config.payout.ghostBonus, .25);
}

TEST(Config, UiTimingLoadsOverridesAndRejectsZeroNegativeAndOverflow) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream console;
    Logger logger(console, "");
    data["ui"] = {{"hover_time", .3}, {"transition_time", .5}};
    auto ui = Config::load(files.write("ui.json", data.dump()), logger).ui;
    EXPECT_FLOAT_EQ(ui.hoverTime, .3f);
    EXPECT_FLOAT_EQ(ui.transitionTime, .5f);
    for (const auto& invalid :
         {nlohmann::json(0), nlohmann::json(-1), nlohmann::json(1e100), nlohmann::json("bad")}) {
        data["ui"] = {{"hover_time", invalid}, {"transition_time", invalid}};
        ui = Config::load(files.write("invalid-ui.json", data.dump()), logger).ui;
        EXPECT_FLOAT_EQ(ui.hoverTime, .15f);
        EXPECT_FLOAT_EQ(ui.transitionTime, .25f);
    }
    data.erase("ui");
    ui = Config::load(files.write("missing-ui.json", data.dump()), logger).ui;
    EXPECT_FLOAT_EQ(ui.hoverTime, .15f);
    EXPECT_NE(console.str().find("[WARN]"), std::string::npos);
}

TEST(Config, PayoutTimingsLoadAndInvalidValuesFallBack) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream output;
    Logger logger(output, "");
    data["ui"]["payout_line_time"] = .4;
    data["ui"]["payout_count_time"] = 2;
    data["ui"]["payout_stamp_time"] = .5;
    auto ui = Config::load(files.write("payout.json", data.dump()), logger).ui;
    EXPECT_FLOAT_EQ(ui.payoutLineTime, .4f);
    EXPECT_FLOAT_EQ(ui.payoutCountTime, 2);
    EXPECT_FLOAT_EQ(ui.payoutStampTime, .5f);
    for (const auto& value :
         {nlohmann::json(0), nlohmann::json(-1), nlohmann::json("bad"), nlohmann::json(1e100)}) {
        for (const char* key : {"payout_line_time", "payout_count_time", "payout_stamp_time"})
            data["ui"][key] = value;
        ui = Config::load(files.write("invalid-payout.json", data.dump()), logger).ui;
        EXPECT_FLOAT_EQ(ui.payoutLineTime, .18f);
        EXPECT_FLOAT_EQ(ui.payoutCountTime, 1.2f);
        EXPECT_FLOAT_EQ(ui.payoutStampTime, .25f);
    }
}

TEST(Config, AudioTimingAndDuckValidateRangesAndWarnWithDefaults) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream output;
    Logger logger(output, "");
    data["audio"] = {{"crossfade_time", 2}, {"voice_duck", .25}};
    auto audio = Config::load(files.write("audio.json", data.dump()), logger).audio;
    EXPECT_FLOAT_EQ(audio.crossfadeTime, 2);
    EXPECT_FLOAT_EQ(audio.voiceDuck, .25f);
    data["audio"]["voice_duck"] = 0;
    EXPECT_FLOAT_EQ(Config::load(files.write("mute.json", data.dump()), logger).audio.voiceDuck, 0);
    for (const auto& bad : {nlohmann::json(-1), nlohmann::json("bad"), nlohmann::json(1e100)}) {
        data["audio"] = {{"crossfade_time", bad}, {"voice_duck", bad}};
        audio = Config::load(files.write("bad-audio.json", data.dump()), logger).audio;
        EXPECT_FLOAT_EQ(audio.crossfadeTime, 1);
        EXPECT_FLOAT_EQ(audio.voiceDuck, .4f);
    }
    data["audio"] = {{"crossfade_time", 0}, {"voice_duck", 1.01}};
    audio = Config::load(files.write("zero-audio.json", data.dump()), logger).audio;
    EXPECT_FLOAT_EQ(audio.crossfadeTime, 1);
    EXPECT_FLOAT_EQ(audio.voiceDuck, .4f);
    EXPECT_NE(output.str().find("[WARN]"), std::string::npos);
}

TEST(Config, VoiceLowHealthThresholdLoadsAndRejectsZeroOrAboveOne) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream out;
    Logger logger(out, "");
    data["voice"]["low_health_fraction"] = .4;
    EXPECT_FLOAT_EQ(
        Config::load(files.write("voice.json", data.dump()), logger).voice.lowHealthFraction, .4f);
    for (float value : {0.f, 1.1f, -1.f}) {
        data["voice"]["low_health_fraction"] = value;
        EXPECT_FLOAT_EQ(Config::load(files.write("invalidvoice.json", data.dump()), logger)
                            .voice.lowHealthFraction,
                        .25f);
    }
    EXPECT_NE(out.str().find("voice.low_health_fraction"), std::string::npos);
}

TEST(Config, Day26PresentationKeysLoadAndInvalidValuesWarnWithDefaults) {
    TestFiles files;
    nlohmann::json data;
    std::ifstream("assets/config/tuning.json") >> data;
    std::ostringstream out;
    Logger logger(out, "");
    data["voice"]["subtitle_words_per_second"] = 0;
    data["ui"]["hint_time"] = -1;
    data["ui"]["panic_flicker_hz"] = 4;
    data["render"]["grain_intensity"] = 2;
    data["render"]["vignette_strength"] = "bad";
    const auto config = Config::load(files.write("day26-invalid.json", data.dump()), logger);
    EXPECT_FLOAT_EQ(config.voice.subtitleWordsPerSecond, 3);
    EXPECT_FLOAT_EQ(config.ui.hintTime, 3);
    EXPECT_FLOAT_EQ(config.ui.panicFlickerHz, 2);
    EXPECT_FLOAT_EQ(config.render.grainIntensity, .04f);
    EXPECT_FLOAT_EQ(config.render.vignetteStrength, .35f);
    EXPECT_NE(out.str().find("panic_flicker_hz"), std::string::npos);
    EXPECT_NE(out.str().find("subtitle_words_per_second"), std::string::npos);
}
