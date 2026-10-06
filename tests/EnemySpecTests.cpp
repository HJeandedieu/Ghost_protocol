#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/Logger.h"
#include "entities/EnemySpec.h"

TEST(EnemySpec, LoadsDocumentedEnemyCatalogWithoutWarnings) {
    std::ostringstream console;
    Logger logger(console, "");
    const auto specs = loadEnemies("assets/config/enemies.json", logger);
    ASSERT_TRUE(specs);
    ASSERT_EQ(specs->size(), 4u);
    const auto& cop = specs->at(1);
    EXPECT_EQ(cop.id, "cop");
    EXPECT_FLOAT_EQ(cop.hp, 100);
    EXPECT_FLOAT_EQ(cop.armor, 0);
    EXPECT_FLOAT_EQ(cop.speed, 170);
    EXPECT_FLOAT_EQ(cop.damage, 7);
    EXPECT_EQ(cop.burst, 3);
    EXPECT_FLOAT_EQ(cop.rate, 1.2f);
    EXPECT_FLOAT_EQ(cop.accuracy, 0.4f);
    EXPECT_FLOAT_EQ(cop.engage, 300);
    EXPECT_FLOAT_EQ(cop.radius, 14);
    EXPECT_EQ(specs->at(0).burst, 1);
    EXPECT_FLOAT_EQ(specs->at(2).shieldArcDeg, 120);
    EXPECT_FLOAT_EQ(specs->at(2).shieldBlock, 0.9f);
    EXPECT_FLOAT_EQ(specs->at(3).armor, 100);
    EXPECT_TRUE(console.str().empty());
}

TEST(EnemySpec, RejectsBadNumbersCountsDuplicatesAndMissingTypes) {
    TestFiles files;
    nlohmann::json original;
    std::ifstream("assets/config/enemies.json") >> original;
    std::ostringstream console;
    Logger logger(console, "");
    for (const auto& field : {"hp", "armor", "speed", "dmg", "rate", "engage"}) {
        auto data = original;
        data["enemies"][1][field] = -1;
        EXPECT_FALSE(loadEnemies(files.write("invalid.json", data.dump()), logger));
    }
    for (const auto& value : {nlohmann::json(2), nlohmann::json(-0.1), nlohmann::json("bad")}) {
        auto data = original;
        data["enemies"][1]["accuracy"] = value;
        EXPECT_FALSE(loadEnemies(files.write("invalid.json", data.dump()), logger));
    }
    for (const auto& value :
         {nlohmann::json(0), nlohmann::json(1.5), nlohmann::json(4294967296LL)}) {
        auto data = original;
        data["enemies"][1]["burst"] = value;
        EXPECT_FALSE(loadEnemies(files.write("invalid.json", data.dump()), logger));
    }
    auto duplicate = original;
    duplicate["enemies"].push_back(duplicate["enemies"][1]);
    EXPECT_FALSE(loadEnemies(files.write("duplicate.json", duplicate.dump()), logger));
    original["enemies"].erase(0);
    EXPECT_FALSE(loadEnemies(files.write("missing-type.json", original.dump()), logger));
    EXPECT_NE(console.str().find("[ERROR]"), std::string::npos);
}

TEST(EnemySpec, RejectsMissingMalformedFilesAndMissingRequiredFields) {
    TestFiles files;
    std::ostringstream console;
    Logger logger(console, "");
    EXPECT_FALSE(loadEnemies(files.path("missing.json"), logger));
    EXPECT_FALSE(loadEnemies(files.write("malformed.json", "{"), logger));
    nlohmann::json data;
    std::ifstream("assets/config/enemies.json") >> data;
    data["enemies"][1].erase("hp");
    EXPECT_FALSE(loadEnemies(files.write("missing-hp.json", data.dump()), logger));
    EXPECT_FALSE(loadEnemies(files.write("bad-root.json", "{\"enemies\":{}}"), logger));
}
TEST(EnemySpec, PoliceCollisionRadiusIsRequiredAndPositive) {
    TestFiles files;
    nlohmann::json original;
    std::ifstream("assets/config/enemies.json") >> original;
    std::ostringstream output;
    Logger logger(output, "");
    for (int type : {1, 2, 3}) {
        auto data = original;
        data["enemies"][type].erase("radius");
        EXPECT_FALSE(loadEnemies(files.write("missing-radius.json", data.dump()), logger));
        for (const auto& value : {nlohmann::json(0), nlohmann::json(-1), nlohmann::json("large")}) {
            data = original;
            data["enemies"][type]["radius"] = value;
            EXPECT_FALSE(loadEnemies(files.write("bad-radius.json", data.dump()), logger));
        }
    }
}
