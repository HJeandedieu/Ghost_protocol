#pragma once
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/Config.h"
class EventBus;
class Logger;
struct VoiceLine {
    std::string id, file, text, trigger;
    int priority = 1;
    float duration = 0;
};
std::optional<std::vector<VoiceLine>> loadVoiceLines(const std::string& path, Logger& logger);
class VoiceDirector {
   public:
    explicit VoiceDirector(std::vector<VoiceLine> lines, float lowHealthFraction,
                           float readingRate = VoiceConfig{}.subtitleWordsPerSecond,
                           float hintTime = UiConfig{}.hintTime);
    void listen(EventBus& events);
    void bindInteraction(std::string interactionId, std::string voiceId);
    void setDuration(const std::string& id, float seconds);
    bool request(const std::string& id, const std::string& occurrence = "");
    void update(float dt);
    void setPaused(bool paused) { paused_ = paused; }
    int tutorialStage() const { return stage_; }
    const std::string& playerId() const { return playerId_; }
    bool paused() const { return paused_; }
    void clear();
    void resetRun();
    void setTutorialContext(int stage, bool enabled, std::string playerId);
    void observeTutorial(bool revealedGuard, bool serviceDoorNearby);
    const std::string& hint() const { return hint_; }
    float hintAge() const { return hintAge_; }
    void observeHealth(float health, float maximum);
    const VoiceLine* current() const;
    float age() const { return age_; }
    unsigned long serial() const { return serial_; }
    std::size_t queued() const { return queue_.size(); }

   private:
    void start(const std::string& id);
    void teach(const std::string& id, const std::string& text, const std::string& voice = "");
    std::map<std::string, VoiceLine> lines_;
    std::map<std::string, std::string> interactions_;
    std::set<std::string> seen_;
    std::deque<std::string> queue_;
    std::string active_;
    float age_ = 0, lowHealthFraction_;
    unsigned long serial_ = 0;
    bool paused_ = false;
    int stage_ = 1;
    bool hintsEnabled_ = true;
    std::string playerId_, hint_;
    std::set<std::string> hintsSeen_;
    std::deque<std::string> hintsQueue_;
    float hintAge_ = 0, hintTime_;
};
