#pragma once

#include "core/Config.h"
#include "core/Vec2.h"

// Pure view state: window/input adapters own pointer capture and delta consumption.
class FirstPersonView {
   public:
    explicit FirstPersonView(ViewConfig config, float yawDeg = 0);
    void look(Vec2 delta);
    Vec2 movement(Vec2 local) const;
    float yawDeg() const { return yawDeg_; }
    float pitchDeg() const { return pitchDeg_; }
    float eyeHeight(bool crouched) const;
    const ViewConfig& config() const { return config_; }

   private:
    ViewConfig config_;
    float yawDeg_ = 0;
    float pitchDeg_ = 0;
};
