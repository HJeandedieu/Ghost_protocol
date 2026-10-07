#include "systems/ScoreSystem.h"

#include <algorithm>
#include <cmath>

#include "core/Rng.h"
void MissionRun::advance(float dt, bool active) {
    if (active && std::isfinite(dt) && dt > 0) seconds += dt;
}
void ScoreSystem::addBag(float value) {
    if (std::isfinite(value) && value > 0) subtotal_ += value;
}
char ScoreSystem::rank(double value) const {
    if (value >= config_.rankS) return 'S';
    if (value >= config_.rankA) return 'A';
    if (value >= config_.rankB) return 'B';
    return 'C';
}
Payout ScoreSystem::finalize(bool ghostRun, double seconds, int deaths, Rng& rng) const {
    Payout result;
    result.subtotal = subtotal_;
    result.ghostBonus = ghostRun ? subtotal_ * config_.ghostBonus : 0;
    result.timeBonus = std::isfinite(seconds) && seconds >= 0 && seconds < config_.timeBonusLimit
                           ? config_.timeBonus
                           : 0;
    const double withBonuses = result.subtotal + result.ghostBonus + result.timeBonus;
    result.handlerCut = withBonuses * config_.handlerCut;
    double deductions = 0;
    const int count = rng.uniformInt(0, config_.deductionMaxCount);
    for (int i = 0; i < count; ++i) {
        const int amount = rng.uniformInt(0, config_.deductionMaxAmount);
        result.deductions.push_back(amount);
        deductions += amount;
    }
    result.deathPenalty = static_cast<double>(std::max(0, deaths)) * config_.deathPenalty;
    result.finalAmount =
        std::max(0.0, withBonuses - result.handlerCut - deductions - result.deathPenalty);
    result.rank = rank(result.finalAmount);
    return result;
}
