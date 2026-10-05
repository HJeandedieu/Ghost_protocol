#pragma once

class Time {
   public:
    static constexpr double kStep = 1.0 / 60.0;
    static constexpr double kMaxFrameTime = 0.25;
    void addFrame(double seconds);
    bool consumeStep();
    float alpha() const;
    void reset();

   private:
    double accumulator_ = 0.0;
};
