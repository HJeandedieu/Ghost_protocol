#include "core/Rng.h"

#include <cmath>
#include <stdexcept>

Rng::Rng(std::uint32_t seed) : seed_(seed), engine_(seed) {}

std::uint32_t Rng::seed() const { return seed_; }

int Rng::uniformInt(int minimum, int maximum) {
    if (minimum > maximum) {
        throw std::invalid_argument("Invalid integer random range");
    }
    return std::uniform_int_distribution<int>(minimum, maximum)(engine_);
}

float Rng::uniformFloat(float minimum, float maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum) {
        throw std::invalid_argument("Invalid floating random range");
    }
    return std::uniform_real_distribution<float>(minimum, maximum)(engine_);
}
