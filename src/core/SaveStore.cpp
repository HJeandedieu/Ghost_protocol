#include "core/SaveStore.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>

#include "core/Logger.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
extern "C" EMSCRIPTEN_KEEPALIVE char* allocateSettingsString(std::size_t size) {
    return static_cast<char*>(std::malloc(size));
}
// clang-format off
EM_JS_DEPS(settingsStorage, "$lengthBytesUTF8,$stringToUTF8,$UTF8ToString");
EM_JS(char*, readSettingsStorage, (const char* key), {
    try {
        const value = localStorage.getItem(UTF8ToString(key));
        if (value === null) return 0;
        const length = lengthBytesUTF8(value) + 1;
        const buffer = _allocateSettingsString(length);
        if (!buffer) return 0;
        stringToUTF8(value, buffer, length);
        return buffer;
    } catch (_) {
        return 0;
    }
});
EM_JS(int, writeSettingsStorage, (const char* key, const char* value), {
    try {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        return 1;
    } catch (_) {
        return 0;
    }
});
#else
// clang-format on
#include <cstdio>
#include <filesystem>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#endif

SaveStore::SaveStore(Logger& logger, std::string path) : logger_(logger), path_(std::move(path)) {}

std::optional<Settings> SaveStore::decodeSettings(const std::string& text) {
    const auto data = nlohmann::json::parse(text, nullptr, false);
    if (!data.is_object()) return std::nullopt;
    Settings settings;
    const auto volume = [&](const char* key, float& output) {
        const auto field = data.find(key);
        if (field == data.end() || !field->is_number()) return false;
        const double value = field->get<double>();
        if (!std::isfinite(value) || value < 0 || value > 1) return false;
        output = static_cast<float>(value);
        return true;
    };
    const auto toggle = [&](const char* key, bool& output) {
        const auto field = data.find(key);
        if (field == data.end() || !field->is_boolean()) return false;
        output = field->get<bool>();
        return true;
    };
    if (!volume("volume_master", settings.volumeMaster) ||
        !volume("volume_music", settings.volumeMusic) ||
        !volume("volume_sfx", settings.volumeSfx) ||
        !volume("volume_voice", settings.volumeVoice) ||
        !toggle("fullscreen", settings.fullscreen) || !toggle("hints", settings.hints) ||
        !toggle("reduce_effects", settings.reduceEffects))
        return std::nullopt;
    const auto difficulty = data.find("difficulty");
    if (difficulty == data.end() || !difficulty->is_string()) return std::nullopt;
    settings.difficulty = difficulty->get<std::string>();
    if (settings.difficulty != "normal" && settings.difficulty != "easy") return std::nullopt;
    return settings;
}

std::string SaveStore::encodeSettings(const Settings& settings) {
    return nlohmann::json{
        {"volume_master", settings.volumeMaster},   {"volume_music", settings.volumeMusic},
        {"volume_sfx", settings.volumeSfx},         {"volume_voice", settings.volumeVoice},
        {"fullscreen", settings.fullscreen},        {"hints", settings.hints},
        {"reduce_effects", settings.reduceEffects}, {"difficulty", settings.difficulty}}
        .dump(2);
}

Settings SaveStore::loadSettings() {
    const auto text = read(path_);
    if (const auto settings = decodeSettings(text)) return *settings;
    logger_.log(LogLevel::Warn, "Missing or corrupt settings; using defaults");
    const Settings defaults;
    saveSettings(defaults);
    return defaults;
}

bool SaveStore::saveSettings(const Settings& settings) {
    const auto text = encodeSettings(settings);
    if (!decodeSettings(text)) {
        logger_.log(LogLevel::Warn, "Invalid settings; save rejected");
        return false;
    }
    if (write(path_, text)) return true;
    logger_.log(LogLevel::Warn, "Cannot save settings; keeping previous stored settings");
    return false;
}

std::string SaveStore::read(const std::string& path) {
    std::string text;
#ifdef __EMSCRIPTEN__
    if (char* stored = readSettingsStorage(path.c_str())) {
        text = stored;
        std::free(stored);
    }
#else
    std::ifstream input(path);
    std::ostringstream content;
    content << input.rdbuf();
    text = content.str();
#endif
    return text;
}
bool SaveStore::write(const std::string& path, const std::string& text) {
#ifdef __EMSCRIPTEN__
    if (writeSettingsStorage(path.c_str(), text.c_str())) return true;
#else
    std::error_code error;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    const auto temporary = path + ".tmp";
    if (!error) {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << text;
        output.flush();
        const bool written = output.good();
        output.close();
        if (written && !output.fail()) {
#ifdef _WIN32
            if (MoveFileExA(temporary.c_str(), path.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return true;
#else
            if (std::rename(temporary.c_str(), path.c_str()) == 0) return true;
#endif
        }
        std::remove(temporary.c_str());
    }
#endif
    return false;
}

std::string SaveStore::encodeScores(const Scores& scores) {
    auto row = [](const ScoreRecord& r) {
        return nlohmann::json{{"best_payout", r.bestPayout},
                              {"best_rank", r.bestRank},
                              {"best_time", r.bestTime},
                              {"ghost_runs", r.ghostRuns}};
    };
    return nlohmann::json{
        {"normal", row(scores.normal)}, {"easy", row(scores.easy)}, {"hard", row(scores.hard)}}
        .dump(2);
}
std::optional<Scores> SaveStore::decodeScores(const std::string& text) {
    auto data = nlohmann::json::parse(text, nullptr, false);
    if (!data.is_object()) return std::nullopt;
    Scores scores;
    for (const auto& name : {"normal", "easy", "hard"}) {
        if (!data.contains(name) || !data[name].is_object()) return std::nullopt;
        const auto& row = data[name];
        for (const auto& key : {"best_payout", "best_time", "ghost_runs"})
            if (!row.contains(key) || !row[key].is_number() ||
                !std::isfinite(row[key].get<double>()) || row[key].get<double>() < 0)
                return std::nullopt;
        if (!row["ghost_runs"].is_number_integer() ||
            row["ghost_runs"].get<double>() > std::numeric_limits<unsigned int>::max())
            return std::nullopt;
        if (!row.contains("best_rank") || !row["best_rank"].is_string()) return std::nullopt;
        auto rank = row["best_rank"].get<std::string>();
        if (rank != "-" && rank != "C" && rank != "B" && rank != "A" && rank != "S")
            return std::nullopt;
        auto& r = std::string(name) == "easy"   ? scores.easy
                  : std::string(name) == "hard" ? scores.hard
                                                : scores.normal;
        r = {row["best_payout"].get<double>(), row["best_time"].get<double>(), rank,
             row["ghost_runs"].get<unsigned int>()};
    }
    return scores;
}
Scores SaveStore::loadScores() {
    const auto path = (std::filesystem::path(path_).parent_path() / "scores.json").generic_string();
    if (auto scores = decodeScores(read(path))) return *scores;
    logger_.log(LogLevel::Warn, "Missing or corrupt scores; using defaults");
    Scores defaults;
    saveScores(defaults);
    return defaults;
}
bool SaveStore::saveScores(const Scores& scores) {
    auto text = encodeScores(scores);
    if (!decodeScores(text)) {
        logger_.log(LogLevel::Warn, "Invalid scores; save rejected");
        return false;
    }
    const auto path = (std::filesystem::path(path_).parent_path() / "scores.json").generic_string();
    if (write(path, text)) return true;
    logger_.log(LogLevel::Warn, "Cannot save scores; keeping previous stored scores");
    return false;
}
