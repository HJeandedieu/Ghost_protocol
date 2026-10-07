#pragma once

#include <optional>
#include <string>

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

   private:
    Logger& logger_;
    std::string path_;
};
