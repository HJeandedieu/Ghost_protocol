#pragma once

#include <cstdint>
#include <random>

class Rng {
   public:
    explicit Rng(std::uint32_t seed);
    std::uint32_t seed() const;
    int uniformInt(int minimum, int maximum);
    float uniformFloat(float minimum, float maximum);

   private:
    std::uint32_t seed_;
    std::mt19937 engine_;
};
