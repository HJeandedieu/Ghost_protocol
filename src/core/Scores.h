#pragma once
#include <string>

struct ScoreRecord {
    double bestPayout = 0, bestTime = 0;
    std::string bestRank = "-";
    unsigned int ghostRuns = 0;
};
struct Scores {
    ScoreRecord normal, easy, hard;
    bool record(const std::string& difficulty, double payout, char rank, double seconds,
                bool ghost);
    const ScoreRecord& forDifficulty(const std::string& difficulty) const;
};
