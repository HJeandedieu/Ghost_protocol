#include "audio/AudioPlayback.h"

#include <utility>

#include "core/Logger.h"
#include "systems/AudioDirector.h"
AudioPlayback::AudioPlayback(Logger& logger, const std::string& root)
    : logger_(logger), root_(root) {
    if (!IsAudioDeviceReady()) {
        logger_.log(LogLevel::Warn, "Audio device unavailable; playback disabled");
        return;
    }
    const char* tracks[] = {"menu_loop", "stealth_loop", "loud_loop", "payout_sting"};
    for (std::size_t i = 0; i < music_.size(); ++i) {
        const auto path = root_ + "/audio/music/" + tracks[i] + ".ogg";
        music_[i] = LoadMusicStream(path.c_str());
        music_[i].looping = i != 3;
        if (!IsMusicValid(music_[i])) logger_.log(LogLevel::Warn, "Cannot load music: " + path);
    }
    const char* names[] = {
        "step_walk",        "step_sprint",   "step_crouch",   "ping_small",    "ping_big",
        "ping_ready",       "lockpick_loop", "door_open",     "gate_open",     "vault_open",
        "keycard_pick",     "bag_pick",      "bag_throw",     "thermite_loop", "drill_pulse",
        "shot_pistol_supp", "shot_smg",      "shot_shotgun",  "reload",        "hit_player",
        "hit_enemy",        "enemy_down",    "takedown",      "pager_ring",    "radio_callin",
        "alarm_siren",      "ui_move",       "ui_select",     "ui_back",       "payout_tick",
        "payout_stamp",     "dye_burst",     "bollard_lower", "van_arrive"};
    for (const auto* name : names) {
        const auto path = root_ + "/audio/sfx/" + name + ".ogg";
        auto sound = LoadSound(path.c_str());
        if (IsSoundValid(sound))
            sounds_.emplace(name, sound);
        else
            logger_.log(LogLevel::Warn, "Cannot load SFX: " + path);
    }
    for (const auto* name : {"step_walk", "step_sprint", "step_crouch", "lockpick_loop",
                             "thermite_loop", "drill_pulse"}) {
        auto stream = LoadMusicStream((root_ + "/audio/sfx/" + name + ".ogg").c_str());
        stream.looping = true;
        if (IsMusicValid(stream)) loops_.emplace(name, stream);
    }
}
AudioPlayback::~AudioPlayback() {
    for (auto& pair : voices_) UnloadSound(pair.second);
    for (auto& pair : sounds_) UnloadSound(pair.second);
    for (auto& pair : loops_) UnloadMusicStream(pair.second);
    for (auto& music : music_)
        if (IsMusicValid(music)) UnloadMusicStream(music);
}
void AudioPlayback::update(AudioDirector& director) {
    if (!IsAudioDeviceReady()) {
        director.takeSounds();
        return;
    }
    SetMasterVolume(director.masterGain());
    voiceGain_ = director.voiceGain();
    const auto weights = director.musicWeights(), gains = director.musicGains();
    for (std::size_t i = 0; i < music_.size(); ++i) {
        if (!IsMusicValid(music_[i])) continue;
        SetMusicVolume(music_[i], gains[i]);
        if (weights[i] > 0) {
            if (!started_[i]) {
                PlayMusicStream(music_[i]);
                started_[i] = true;
            }
            UpdateMusicStream(music_[i]);
        } else if (started_[i]) {
            StopMusicStream(music_[i]);
            started_[i] = false;
        }
    }
    for (auto& pair : sounds_) SetSoundVolume(pair.second, director.sfxGain());
    for (auto& pair : voices_) SetSoundVolume(pair.second, voiceGain_);
    for (const auto& name : director.takeSounds()) {
        const auto found = sounds_.find(name);
        if (found != sounds_.end())
            PlaySound(found->second);
        else if (warned_.insert(name).second)
            logger_.log(LogLevel::Warn, "Unknown or missing SFX: " + name);
    }
    for (auto& pair : loops_) {
        SetMusicVolume(pair.second, director.sfxGain());
        if (director.loops().count(pair.first)) {
            if (!activeLoops_.count(pair.first)) PlayMusicStream(pair.second);
            UpdateMusicStream(pair.second);
        } else if (activeLoops_.count(pair.first))
            StopMusicStream(pair.second);
    }
    activeLoops_ = director.loops();
}
bool AudioPlayback::playVoice(const std::string& id) {
    if (!IsAudioDeviceReady()) return false;
    if (id.size() != 3 || id[0] != 'V' || id[1] < '0' || id[1] > '2' || id[2] < '0' ||
        id[2] > '9' || id < "V01" || id > "V25")
        return false;
    auto found = voices_.find(id);
    if (found == voices_.end()) {
        auto sound = LoadSound((root_ + "/audio/voice/" + id + ".ogg").c_str());
        if (!IsSoundValid(sound)) {
            if (warned_.insert(id).second) logger_.log(LogLevel::Warn, "Cannot load voice: " + id);
            return false;
        }
        found = voices_.emplace(id, sound).first;
    }
    stopVoice();
    SetSoundVolume(found->second, voiceGain_);
    PlaySound(found->second);
    return true;
}
void AudioPlayback::stopVoice() {
    for (auto& pair : voices_) StopSound(pair.second);
}
bool AudioPlayback::voicePlaying() const {
    for (const auto& pair : voices_)
        if (IsSoundPlaying(pair.second)) return true;
    return false;
}
