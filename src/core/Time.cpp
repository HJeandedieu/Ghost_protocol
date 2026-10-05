#include "core/Time.h"

#include <algorithm>
#include <cmath>

void Time::addFrame(double seconds) {
    if (std::isfinite(seconds) && seconds > 0.0) {
        accumulator_ += std::min(seconds, kMaxFrameTime);
    }
}

bool Time::consumeStep() {
    if (accumulator_ < kStep) {
        return false;
    }
    accumulator_ -= kStep;
    return true;
}

float Time::alpha() const { return static_cast<float>(accumulator_ / kStep); }

void Time::reset() { accumulator_ = 0.0; }
