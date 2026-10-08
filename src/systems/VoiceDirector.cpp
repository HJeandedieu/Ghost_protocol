#include "systems/VoiceDirector.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "core/EventBus.h"
#include "core/Events.h"
#include "core/Logger.h"
std::optional<std::vector<VoiceLine>> loadVoiceLines(const std::string& path, Logger& logger) {
    try {
        std::ifstream input(path);
        nlohmann::json data;
        input >> data;
        if (!data.is_array() || data.size() != 25) throw std::runtime_error("expected 25 lines");
        std::set<std::string> ids;
        std::vector<VoiceLine> lines;
        for (const auto& entry : data) {
            VoiceLine line;
            line.id = entry.at("id").get<std::string>();
            line.file = entry.at("file").get<std::string>();
            line.text = entry.at("text").get<std::string>();
            line.trigger = entry.at("trigger").get<std::string>();
            if (!entry.at("priority").is_number_integer() || entry.at("priority") < 1 ||
                entry.at("priority") > 3)
                throw std::runtime_error("priority");
            line.priority = entry.at("priority").get<int>();
            if (line.id.size() != 3 || line.id < "V01" || line.id > "V25" || line.id[0] != 'V' ||
                line.id[1] < '0' || line.id[1] > '2' || line.id[2] < '0' || line.id[2] > '9' ||
                !ids.insert(line.id).second || line.file != "audio/voice/" + line.id + ".ogg" ||
                line.text.empty() || line.trigger.empty() || line.priority < 1 || line.priority > 3)
                throw std::runtime_error("invalid voice entry");
            lines.push_back(std::move(line));
        }
        return lines;
    } catch (const std::exception& error) {
        logger.log(LogLevel::Warn, "Cannot load voice manifest: " + path + ": " + error.what());
        return std::nullopt;
    }
}
VoiceDirector::VoiceDirector(std::vector<VoiceLine> lines, float lowHealthFraction,
                             float readingRate, float hintTime)
    : lowHealthFraction_(lowHealthFraction), hintTime_(hintTime) {
    if (!std::isfinite(readingRate) || readingRate <= 0)
        readingRate = VoiceConfig{}.subtitleWordsPerSecond;
    if (!std::isfinite(hintTime_) || hintTime_ <= 0) hintTime_ = UiConfig{}.hintTime;
    for (auto& line : lines) {
        if (!std::isfinite(line.duration) || line.duration <= 0) {
            std::istringstream words(line.text);
            std::string word;
            int count = 0;
            while (words >> word) ++count;
            line.duration = static_cast<float>(
                std::min(static_cast<double>(std::max(1, count)) / readingRate,
                         static_cast<double>(std::numeric_limits<float>::max())));
        }
        const auto id = line.id;
        lines_.emplace(id, std::move(line));
    }
}
void VoiceDirector::bindInteraction(std::string id, std::string voice) {
    interactions_[std::move(id)] = std::move(voice);
}
void VoiceDirector::listen(EventBus& events) {
    interactions_.clear();
    events.subscribe<GuardTakenDown>([this](const auto&) { request("V06"); });
    events.subscribe<NoiseEmitted>([this](const auto& e) {
        if (e.type == NoiseType::Ping && e.sourceId == playerId_)
            teach("crouch", "C / CTRL: crouch to move quietly", "V04");
    });
    events.subscribe<PagerRang>([this](const auto& e) {
        if (stage_ <= 2) {
            if (hintsEnabled_) {
                teach("pager", "Hold E beside the body to answer its pager");
                request("V07", e.bodyId);
            }
        } else
            request("V07", e.bodyId);
    });
    events.subscribe<PagerMissed>([this](const auto& e) { request("V08", e.bodyId); });
    events.subscribe<SecurityLooped>([this](const auto&) { request("V10"); });
    events.subscribe<AlarmTriggered>([this](const auto&) { request("V13"); });
    events.subscribe<WaveSpawned>([this](const auto&) { request("V14"); });
    events.subscribe<BagPicked>([this](const auto&) { request("V19"); });
    events.subscribe<InteractionDone>([this](const auto& e) {
        const auto found = interactions_.find(e.interactableId);
        if (found != interactions_.end()) request(found->second);
    });
}
void VoiceDirector::setDuration(const std::string& id, float seconds) {
    const auto found = lines_.find(id);
    if (found != lines_.end() && std::isfinite(seconds) && seconds > 0)
        found->second.duration = seconds;
}
const VoiceLine* VoiceDirector::current() const {
    const auto found = lines_.find(active_);
    return found == lines_.end() ? nullptr : &found->second;
}
void VoiceDirector::start(const std::string& id) {
    active_ = id;
    age_ = 0;
    ++serial_;
}
bool VoiceDirector::request(const std::string& id, const std::string& occurrence) {
    const auto found = lines_.find(id);
    if (found == lines_.end() || found->second.duration <= 0) return false;
    const auto key = id + ":" + ((id == "V07" || id == "V08") ? occurrence : "");
    if (!seen_.insert(key).second) return false;
    const auto* playing = current();
    if (!playing || found->second.priority == 3 ||
        (found->second.priority == 2 && playing->priority == 1))
        start(id);
    else
        queue_.push_back(id);
    return true;
}
void VoiceDirector::update(float dt) {
    if (paused_ || !std::isfinite(dt) || dt <= 0) return;
    if (!hint_.empty()) {
        hintAge_ += dt;
        while (hintAge_ >= hintTime_ && !hint_.empty()) {
            hintAge_ -= hintTime_;
            if (hintsQueue_.empty()) {
                hint_.clear();
                hintAge_ = 0;
            } else {
                hint_ = hintsQueue_.front();
                hintsQueue_.pop_front();
            }
        }
    }
    while (const auto* line = current()) {
        const float remaining = line->duration - age_;
        if (dt < remaining) {
            age_ += dt;
            return;
        }
        dt -= remaining;
        active_.clear();
        age_ = 0;
        ++serial_;
        if (queue_.empty()) return;
        auto next = queue_.front();
        queue_.pop_front();
        start(next);
        if (dt <= 0) return;
    }
}
void VoiceDirector::clear() {
    active_.clear();
    queue_.clear();
    age_ = 0;
    ++serial_;
    paused_ = false;
    hint_.clear();
    hintsQueue_.clear();
    hintAge_ = 0;
}
void VoiceDirector::resetRun() {
    clear();
    seen_.clear();
    interactions_.clear();
    hintsSeen_.clear();
}
void VoiceDirector::observeHealth(float health, float maximum) {
    if (std::isfinite(health) && std::isfinite(maximum) && health > 0 && maximum > 0 &&
        health <= maximum * lowHealthFraction_)
        request("V24");
}

void VoiceDirector::setTutorialContext(int stage, bool enabled, std::string playerId) {
    stage_ = stage;
    hintsEnabled_ = enabled;
    playerId_ = std::move(playerId);
    if (!enabled || stage > 2) {
        hint_.clear();
        hintsQueue_.clear();
        hintAge_ = 0;
    }
    if (!enabled || stage > 2) {
        const auto tutorial = [stage](const std::string& id) {
            return id == "V03" || id == "V04" || id == "V05" || (stage <= 2 && id == "V07");
        };
        queue_.erase(std::remove_if(queue_.begin(), queue_.end(), tutorial), queue_.end());
        if (tutorial(active_)) {
            active_.clear();
            age_ = 0;
            ++serial_;
            if (!queue_.empty()) {
                auto next = queue_.front();
                queue_.pop_front();
                start(next);
            }
        }
    }
    if (stage == 1) teach("ping", "SPACE: tap to ping, hold for a larger reveal", "V03");
}
void VoiceDirector::teach(const std::string& id, const std::string& text,
                          const std::string& voice) {
    if (!hintsEnabled_ || stage_ > 2 || !hintsSeen_.insert(id).second) return;
    if (hint_.empty()) {
        hint_ = text;
        hintAge_ = 0;
    } else
        hintsQueue_.push_back(text);
    if (!voice.empty()) request(voice);
}
void VoiceDirector::observeTutorial(bool guard, bool door) {
    if (guard) teach("takedown", "RIGHT CLICK behind a guard: quiet takedown", "V05");
    if (door) teach("interact", "Hold E beside the service door to pick the lock");
}
