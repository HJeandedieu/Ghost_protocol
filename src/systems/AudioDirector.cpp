#include "systems/AudioDirector.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/EventBus.h"
AudioDirector::AudioDirector(AudioConfig config) : config_(config) {
    if (!std::isfinite(config_.crossfadeTime) || config_.crossfadeTime <= 0)
        config_.crossfadeTime = AudioConfig{}.crossfadeTime;
    if (!std::isfinite(config_.voiceDuck) || config_.voiceDuck < 0 || config_.voiceDuck > 1)
        config_.voiceDuck = AudioConfig{}.voiceDuck;
}
void AudioDirector::setScene(AudioScene scene) {
    scene_ = scene;
    fading_ = false;
    fadeAge_ = 0;
    clearLoops();
    sounds_.clear();
    interactions_.clear();
}
void AudioDirector::setVolumes(float master, float music, float sfx, float voice) {
    const auto valid = [](float value) {
        return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
    };
    master_ = valid(master);
    music_ = valid(music);
    sfx_ = valid(sfx);
    voice_ = valid(voice);
}
void AudioDirector::update(float dt, bool voicePlaying) {
    voicePlaying_ = voicePlaying;
    if (!std::isfinite(dt) || dt <= 0) return;
    if (fading_) {
        fadeAge_ = std::min(config_.crossfadeTime, fadeAge_ + dt);
        if (fadeAge_ >= config_.crossfadeTime) fading_ = false;
    }
}
std::array<float, 4> AudioDirector::musicWeights() const {
    std::array<float, 4> result{};
    if (scene_ == AudioScene::Menu) result[0] = 1;
    if (scene_ == AudioScene::Stealth) result[1] = 1;
    if (scene_ == AudioScene::Loud) {
        const float t = fading_ ? fadeAge_ / config_.crossfadeTime : 1;
        result[1] = 1 - t;
        result[2] = t;
    }
    if (scene_ == AudioScene::Payout) result[3] = 1;
    return result;
}
std::array<float, 4> AudioDirector::musicGains() const {
    auto result = musicWeights();
    for (auto& gain : result) gain *= music_ * (voicePlaying_ ? config_.voiceDuck : 1);
    return result;
}
void AudioDirector::request(const std::string& id) {
    if (!id.empty()) sounds_.push_back(id);
}
std::vector<std::string> AudioDirector::takeSounds() {
    auto result = std::move(sounds_);
    sounds_.clear();
    return result;
}
void AudioDirector::bindInteraction(std::string id, std::string sound) {
    interactions_[std::move(id)] = std::move(sound);
}
void AudioDirector::listen(EventBus& events) {
    events.subscribe<AlarmTriggered>([this](const auto&) {
        if (scene_ == AudioScene::Stealth) {
            scene_ = AudioScene::Loud;
            fading_ = true;
            fadeAge_ = 0;
            request("alarm_siren");
        }
    });
    events.subscribe<ShotFired>([this](const auto& e) {
        request(e.weaponId == "whisper" ? "shot_pistol_supp"
                : e.weaponId == "gavel" ? "shot_shotgun"
                                        : "shot_smg");
    });
    events.subscribe<EntityDamaged>([this](const auto& e) {
        if (e.amount > 0) request(e.targetId == "Ghost" ? "hit_player" : "hit_enemy");
    });
    events.subscribe<EntityDied>([this](const auto& e) {
        if (e.targetId != "Ghost") request("enemy_down");
    });
    events.subscribe<GuardTakenDown>([this](const auto&) { request("takedown"); });
    events.subscribe<PagerRang>([this](const auto&) { request("pager_ring"); });
    events.subscribe<CallInStarted>([this](const auto&) { request("radio_callin"); });
    events.subscribe<BagPicked>([this](const auto&) { request("bag_pick"); });
    events.subscribe<BagDropped>([this](const auto&) { request("bag_throw"); });
    events.subscribe<BagDelivered>([this](const auto&) { request("bag_pick"); });
    events.subscribe<DyePackBurst>([this](const auto&) { request("dye_burst"); });
    events.subscribe<InteractionDone>([this](const auto& e) {
        const auto found = interactions_.find(e.interactableId);
        if (found != interactions_.end()) request(found->second);
    });
}
