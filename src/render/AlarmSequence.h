#pragma once
#include "core/Config.h"
#include "core/Rng.h"
#include "core/Vec2.h"
class EventBus;
class AlarmSequence {
   public:
    AlarmSequence(EventBus& events, const AlarmConfig& config, std::uint32_t seed);
    float advance(float realDt);
    float paletteBlend() const;
    float barsFraction() const;
    float vignettePulse() const;
    bool bannerVisible() const;
    Vec2 shakeOffset(bool reduced) const;
    float shakeAmplitude(bool reduced) const;
    double elapsed() const { return elapsed_; }
    bool started() const { return started_; }

   private:
    AlarmConfig config_;
    Rng rng_;
    double elapsed_ = 0;
    bool started_ = false;
    Vec2 direction_{};
};
