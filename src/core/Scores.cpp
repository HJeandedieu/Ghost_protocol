#include "core/Scores.h"

#include <cmath>
#include <limits>

const ScoreRecord& Scores::forDifficulty(const std::string& difficulty) const {
    return difficulty == "easy" ? easy : difficulty == "hard" ? hard : normal;
}
bool Scores::record(const std::string& difficulty, double payout, char rank, double seconds,
                    bool ghost) {
    if ((difficulty != "normal" && difficulty != "easy" && difficulty != "hard") ||
        !std::isfinite(payout) || payout < 0 || !std::isfinite(seconds) || seconds < 0 ||
        std::string("CBAS").find(rank) == std::string::npos)
        return false;
    auto& target = difficulty == "easy" ? easy : difficulty == "hard" ? hard : normal;
    if (payout > target.bestPayout) {
        target.bestPayout = payout;
        target.bestRank = std::string(1, rank);
    }
    if (target.bestTime == 0 || seconds < target.bestTime) target.bestTime = seconds;
    if (ghost && target.ghostRuns < std::numeric_limits<unsigned int>::max()) ++target.ghostRuns;
    return true;
}
