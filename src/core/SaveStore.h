#pragma once

#include <optional>
#include <string>

#include "core/Scores.h"

class Logger;

struct Settings {
    float volumeMaster = .8f;
    float volumeMusic = .7f;
    float volumeSfx = .8f;
    float volumeVoice = 1;
    bool fullscreen = false;
    bool hints = true;
    bool reduceEffects = false;
    std::string difficulty = "normal";
};

class SaveStore {
   public:
    explicit SaveStore(Logger& logger, std::string path = "save/settings.json");
    Settings loadSettings();
    bool saveSettings(const Settings& settings);
    static std::optional<Settings> decodeSettings(const std::string& json);
    static std::string encodeSettings(const Settings& settings);
    Scores loadScores();
    bool saveScores(const Scores& scores);
    static std::optional<Scores> decodeScores(const std::string& json);
    static std::string encodeScores(const Scores& scores);

   private:
    Logger& logger_;
    std::string path_;
    std::string read(const std::string& path);
    bool write(const std::string& path, const std::string& text);
};
