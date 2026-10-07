#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "TestFiles.h"
#include "core/Logger.h"
#include "core/SaveStore.h"

TEST(SaveStore, SettingsSurviveNewStoreAndAtomicReplacement) {
    TestFiles files;
    std::ostringstream output;
    Logger logger(output, "");
    SaveStore store(logger, files.path("save/settings.json"));
    Settings settings;
    settings.volumeMaster = 0;
    settings.volumeMusic = 1;
    settings.volumeSfx = .25f;
    settings.volumeVoice = .5f;
    settings.fullscreen = true;
    settings.hints = false;
    settings.reduceEffects = true;
    settings.difficulty = "easy";
    ASSERT_TRUE(store.saveSettings(settings));
    SaveStore restarted(logger, files.path("save/settings.json"));
    auto loaded = restarted.loadSettings();
    EXPECT_FLOAT_EQ(loaded.volumeMaster, 0);
    EXPECT_FLOAT_EQ(loaded.volumeMusic, 1);
    EXPECT_FLOAT_EQ(loaded.volumeSfx, .25f);
    EXPECT_FLOAT_EQ(loaded.volumeVoice, .5f);
    EXPECT_TRUE(loaded.fullscreen);
    EXPECT_FALSE(loaded.hints);
    EXPECT_TRUE(loaded.reduceEffects);
    EXPECT_EQ(loaded.difficulty, "easy");
    settings.volumeMaster = .4f;
    ASSERT_TRUE(restarted.saveSettings(settings));
    EXPECT_FLOAT_EQ(store.loadSettings().volumeMaster, .4f);
}

TEST(SaveStore, MissingAndCorruptFilesAreReplacedWithWarnedDefaults) {
    TestFiles files;
    std::ostringstream output;
    Logger logger(output, "");
    SaveStore store(logger, files.path("settings.json"));
    EXPECT_FLOAT_EQ(store.loadSettings().volumeMaster, .8f);
    files.write("settings.json", "{broken");
    EXPECT_EQ(store.loadSettings().difficulty, "normal");
    EXPECT_TRUE(SaveStore::decodeSettings(SaveStore::encodeSettings(store.loadSettings())));
    EXPECT_NE(output.str().find("[WARN] Missing or corrupt settings"), std::string::npos);
}

TEST(SaveStore, InvalidSavePreservesLastValidSettings) {
    TestFiles files;
    std::ostringstream output;
    Logger logger(output, "");
    SaveStore store(logger, files.path("settings.json"));
    Settings settings;
    ASSERT_TRUE(store.saveSettings(settings));
    for (const float invalid : {-1.f, 2.f, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        settings.volumeMaster = invalid;
        EXPECT_FALSE(store.saveSettings(settings));
        EXPECT_FLOAT_EQ(store.loadSettings().volumeMaster, .8f);
    }
    EXPECT_FALSE(SaveStore::decodeSettings("[]"));
    EXPECT_FALSE(SaveStore::decodeSettings("{}"));
    settings = Settings{};
    settings.difficulty = "unknown";
    EXPECT_FALSE(store.saveSettings(settings));
}

TEST(SaveStore, UnwritableParentReportsFailureWithoutCrashing) {
    TestFiles files;
    const auto parent = files.write("parent", "not a directory");
    std::ostringstream output;
    Logger logger(output, "");
    SaveStore store(logger, parent + "/settings.json");
    EXPECT_FALSE(store.saveSettings(Settings{}));
    EXPECT_FLOAT_EQ(store.loadSettings().volumeMaster, .8f);
    EXPECT_NE(output.str().find("[WARN] Cannot save settings"), std::string::npos);
}
