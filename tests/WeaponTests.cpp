#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/Logger.h"
#include "entities/Weapon.h"
namespace {
std::vector<WeaponSpec> shipped() {
    std::ostringstream console;
    Logger logger(console, "");
    return loadWeapons("assets/config/weapons.json", logger).value();
}
}  // namespace
TEST(Weapon, LoadsAllThreeDocumentedWeapons) {
    const auto specs = shipped();
    ASSERT_EQ(specs.size(), 3u);
    EXPECT_EQ(specs[0].id, "whisper");
    EXPECT_FLOAT_EQ(specs[0].damage, 22);
    EXPECT_EQ(specs[0].magazine, 12);
    EXPECT_EQ(specs[0].noise, NoiseType::ShotSupp);
    EXPECT_EQ(specs[1].reserve, 150);
    EXPECT_FLOAT_EQ(specs[1].rate, 12);
    EXPECT_EQ(specs[2].pellets, 8);
    EXPECT_FLOAT_EQ(specs[2].spreadDeg, 18);
}
TEST(Weapon, RejectsBrokenFilesMissingWeaponsInvalidCountsAndUnknownNoise) {
    TestFiles files;
    std::ostringstream console;
    Logger logger(console, "");
    EXPECT_FALSE(loadWeapons(files.write("bad.json", "{"), logger));
    EXPECT_FALSE(loadWeapons("assets/config/nonexistent-weapons.json", logger));
    nlohmann::json data;
    std::ifstream("assets/config/weapons.json") >> data;
    for (const auto& invalid : {nlohmann::json(0), nlohmann::json(-1), nlohmann::json(2.5)}) {
        data["weapons"][0]["mag"] = invalid;
        EXPECT_FALSE(loadWeapons(files.write("invalid.json", data.dump()), logger));
    }
    std::ifstream("assets/config/weapons.json") >> data;
    data["weapons"][0]["noise"] = "ping";
    EXPECT_FALSE(loadWeapons(files.write("noise.json", data.dump()), logger));
    data["weapons"].erase(data["weapons"].begin());
    EXPECT_FALSE(loadWeapons(files.write("missing.json", data.dump()), logger));
}
TEST(Weapon, RateLimitsHeldFireAtSixtyTicksAndConsumesOnlyAcceptedShots) {
    for (const auto& spec : shipped()) {
        Weapon weapon(spec);
        int shots = 0;
        for (int tick = 0; tick < 60; ++tick) {
            if (tick > 0) weapon.update(1.0f / 60);
            if (weapon.consumeShot()) ++shots;
        }
        EXPECT_EQ(shots, static_cast<int>(std::ceil(spec.rate)));
        EXPECT_EQ(weapon.ammunition(), spec.magazine - shots);
    }
}
TEST(Weapon, ReloadTransfersOnlyAtCompletionAndRejectsFireDuringReload) {
    Weapon weapon(shipped()[0]);
    EXPECT_FALSE(weapon.beginReload());
    ASSERT_TRUE(weapon.consumeShot());
    ASSERT_TRUE(weapon.beginReload());
    EXPECT_FALSE(weapon.beginReload());
    weapon.update(1.399f);
    EXPECT_EQ(weapon.ammunition(), 11);
    EXPECT_EQ(weapon.reserve(), 60);
    EXPECT_FALSE(weapon.consumeShot());
    weapon.update(0.001f);
    EXPECT_EQ(weapon.ammunition(), 12);
    EXPECT_EQ(weapon.reserve(), 59);
    EXPECT_TRUE(weapon.consumeShot());
}
TEST(Weapon, EmptyMagazineAndPartialReserveNeverCreateAmmunition) {
    auto spec = shipped()[0];
    spec.reserve = 2;
    Weapon weapon(spec);
    for (int i = 0; i < spec.magazine; ++i) {
        ASSERT_TRUE(weapon.consumeShot());
        weapon.update(1 / spec.rate);
    }
    EXPECT_FALSE(weapon.consumeShot());
    ASSERT_TRUE(weapon.beginReload());
    weapon.update(spec.reload);
    EXPECT_EQ(weapon.ammunition(), 2);
    EXPECT_EQ(weapon.reserve(), 0);
    EXPECT_FALSE(weapon.beginReload());
}
TEST(Weapon, InvalidTimeCannotFinishReloadOrBypassRateLimit) {
    Weapon weapon(shipped()[0]);
    ASSERT_TRUE(weapon.consumeShot());
    weapon.update(-1);
    weapon.update(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FALSE(weapon.consumeShot());
    ASSERT_TRUE(weapon.beginReload());
    weapon.update(-1);
    EXPECT_FLOAT_EQ(weapon.reloadRemaining(), weapon.spec().reload);
}
