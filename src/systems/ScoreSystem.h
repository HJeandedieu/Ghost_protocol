#pragma once
#include <vector>

#include "core/Config.h"
class Rng;
struct Payout {
    double subtotal = 0, ghostBonus = 0, timeBonus = 0, handlerCut = 0, deathPenalty = 0,
           finalAmount = 0;
    std::vector<int> deductions;
    char rank = 'C';
};
struct MissionRun {
    double seconds = 0;
    int deaths = 0;
    bool alarmEver = false;
    void advance(float dt, bool active);
};
class ScoreSystem {
   public:
    explicit ScoreSystem(const PayoutConfig& config) : config_(config) {}
    void addBag(float value);
    Payout finalize(bool ghostRun, double seconds, int deaths, Rng& rng) const;
    char rank(double finalPayout) const;

   private:
    PayoutConfig config_;
    double subtotal_ = 0;
};
