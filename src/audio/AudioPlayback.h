#pragma once
#include <array>
#include <map>
#include <set>
#include <string>

#include "raylib.h"
class Logger;
class AudioDirector;
class AudioPlayback {
   public:
    explicit AudioPlayback(Logger& logger, const std::string& root = "assets");
    ~AudioPlayback();
    AudioPlayback(const AudioPlayback&) = delete;
    AudioPlayback& operator=(const AudioPlayback&) = delete;
    void update(AudioDirector& director);
    bool playVoice(const std::string& id);
    void stopVoice();
    bool voicePlaying() const;

   private:
    Logger& logger_;
    std::string root_;
    std::array<Music, 4> music_{};
    std::array<bool, 4> started_{};
    std::map<std::string, Sound> sounds_, voices_;
    std::map<std::string, Music> loops_;
    std::set<std::string> activeLoops_, warned_;
    float voiceGain_ = 1;
};
