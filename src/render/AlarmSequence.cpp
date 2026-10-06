#include "render/AlarmSequence.h"

#include <algorithm>
#include <cmath>

#include "core/EventBus.h"
AlarmSequence::AlarmSequence(EventBus& events, const AlarmConfig& config, std::uint32_t seed)
    : config_(config), rng_(seed) {
    events.subscribe<AlarmTriggered>([this](const auto&) {
        if (started_) return;
        started_ = true;
        direction_ = {rng_.uniformFloat(-1, 1), rng_.uniformFloat(-1, 1)};
        const float length = std::hypot(direction_.x, direction_.y);
        if (length > 1) {
            direction_.x /= length;
            direction_.y /= length;
        }
    });
}
void AlarmSequence::restoreLoud() {
    started_ = true;
    restored_ = true;
    elapsed_ = std::max(
        {config_.flipTime, config_.barsIn, config_.slowmoTime + config_.barsOut, config_.bannerTime,
         config_.traumaDecay > 0 ? config_.shakeTrauma / config_.traumaDecay : 0.f});
    direction_ = {};
}

float AlarmSequence::advance(float realDt) {
    if (!std::isfinite(realDt) || realDt <= 0) return 0;
    if (!started_ || restored_) return realDt;
    const double slow = std::min(static_cast<double>(realDt),
                                 std::max(0.0, static_cast<double>(config_.slowmoTime) - elapsed_));
    elapsed_ += realDt;
    direction_ = {rng_.uniformFloat(-1, 1), rng_.uniformFloat(-1, 1)};
    const float length = std::hypot(direction_.x, direction_.y);
    if (length > 1) {
        direction_.x /= length;
        direction_.y /= length;
    }
    return static_cast<float>(slow * config_.slowmoScale + realDt - slow);
}
float AlarmSequence::paletteBlend() const {
    if (!started_) return 0;
    return config_.flipTime > 0
               ? static_cast<float>(std::clamp(elapsed_ / config_.flipTime, 0.0, 1.0))
               : 1;
}
float AlarmSequence::barsFraction() const {
    if (!started_) return 0;
    if (elapsed_ < config_.barsIn && config_.barsIn > 0) {
        const float remaining = 1 - static_cast<float>(elapsed_ / config_.barsIn);
        return 1 - remaining * remaining * remaining;
    }
    if (elapsed_ <= config_.slowmoTime) return 1;
    if (config_.barsOut <= 0) return 0;
    const float remaining = static_cast<float>(
        std::clamp(1 - (elapsed_ - config_.slowmoTime) / config_.barsOut, 0.0, 1.0));
    return remaining * remaining * remaining;
}
float AlarmSequence::vignettePulse() const {
    const float duration = config_.slowmoTime + config_.barsOut;
    if (!started_ || duration <= 0 || elapsed_ >= duration) return 0;
    return std::sin(static_cast<float>(elapsed_ / duration) * 3.14159265358979323846f);
}
bool AlarmSequence::bannerVisible() const { return started_ && elapsed_ < config_.bannerTime; }
float AlarmSequence::shakeAmplitude(bool reduced) const {
    if (!started_ || restored_) return 0;
    const float trauma =
        std::max(0.f, config_.shakeTrauma - static_cast<float>(elapsed_) * config_.traumaDecay);
    return config_.shakePixels * trauma * trauma * (reduced ? 0.5f : 1.f);
}
Vec2 AlarmSequence::shakeOffset(bool reduced) const {
    const float amplitude = shakeAmplitude(reduced);
    return {direction_.x * amplitude, direction_.y * amplitude};
}
