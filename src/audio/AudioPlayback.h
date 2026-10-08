#pragma once
#include <array>
#include <map>
#include <set>
#include <string>

#include "raylib.h"
class Logger;
class AudioDirector;
class VoiceDirector;
class AudioPlayback {
   public:
    explicit AudioPlayback(Logger& logger, const std::string& root = "assets");
    ~AudioPlayback();
    AudioPlayback(const AudioPlayback&) = delete;
    AudioPlayback& operator=(const AudioPlayback&) = delete;
    void update(AudioDirector& director);
    void updateVoice(const VoiceDirector& director);
    float voiceDuration(const std::string& id);
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
    unsigned long voiceSerial_ = 0;
    bool voicePaused_ = false;
    std::string activeVoice_;
    std::map<std::string, float> durations_;
};
