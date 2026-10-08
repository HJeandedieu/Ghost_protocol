#pragma once
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/Config.h"
class EventBus;
enum class AudioScene { Silent, Menu, Stealth, Loud, Payout };
class AudioDirector {
   public:
    explicit AudioDirector(AudioConfig config);
    void listen(EventBus& events);
    void setScene(AudioScene scene);
    void setVolumes(float master, float music, float sfx, float voice);
    void update(float dt, bool voicePlaying);
    void request(const std::string& id);
    std::vector<std::string> takeSounds();
    void clearLoops() { loops_.clear(); }
    void loop(const std::string& id) { loops_.insert(id); }
    void bindInteraction(std::string id, std::string sound);
    const std::set<std::string>& loops() const { return loops_; }
    std::array<float, 4> musicWeights() const;
    std::array<float, 4> musicGains() const;
    float masterGain() const { return master_; }
    float sfxGain() const { return sfx_; }
    float voiceGain() const { return voice_; }
    AudioScene scene() const { return scene_; }

   private:
    AudioConfig config_;
    AudioScene scene_ = AudioScene::Silent;
    float master_ = 1, music_ = 1, sfx_ = 1, voice_ = 1;
    float fadeAge_ = 0;
    bool fading_ = false, voicePlaying_ = false;
    std::vector<std::string> sounds_;
    std::set<std::string> loops_;
    std::map<std::string, std::string> interactions_;
};
