#include "core/SaveStore.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
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
    std::string text;
#ifdef __EMSCRIPTEN__
    if (char* stored = readSettingsStorage(path_.c_str())) {
        text = stored;
        std::free(stored);
    }
#else
    std::ifstream input(path_);
    std::ostringstream content;
    content << input.rdbuf();
    text = content.str();
#endif
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
#ifdef __EMSCRIPTEN__
    if (writeSettingsStorage(path_.c_str(), text.c_str())) return true;
#else
    std::error_code error;
    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    const auto temporary = path_ + ".tmp";
    if (!error) {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << text;
        output.flush();
        const bool written = output.good();
        output.close();
        if (written && !output.fail()) {
#ifdef _WIN32
            if (MoveFileExA(temporary.c_str(), path_.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return true;
#else
            if (std::rename(temporary.c_str(), path_.c_str()) == 0) return true;
#endif
        }
        std::remove(temporary.c_str());
    }
#endif
    logger_.log(LogLevel::Warn, "Cannot save settings; keeping previous stored settings");
    return false;
}
