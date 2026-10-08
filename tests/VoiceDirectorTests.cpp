#include <gtest/gtest.h>

#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>

#include "TestFiles.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/Logger.h"
#include "systems/VoiceDirector.h"
namespace {
std::vector<VoiceLine> lines() {
    return {{"V02", "", "quiet", "", 1, 2},  {"V03", "", "teach", "", 2, 3},
            {"V04", "", "crouch", "", 1, 2}, {"V07", "", "pager", "", 2, 2},
            {"V08", "", "missed", "", 3, 2}, {"V13", "", "alarm", "", 3, 2},
            {"V24", "", "health", "", 2, 2}};
}
}  // namespace
TEST(VoiceDirector, PriorityTwoInterruptsOneAndFifoSurvivesPanic) {
    VoiceDirector voice(lines(), .25f);
    ASSERT_TRUE(voice.request("V02"));
    voice.update(1);
    ASSERT_TRUE(voice.request("V03"));
    EXPECT_EQ(voice.current()->id, "V03");
    EXPECT_FLOAT_EQ(voice.age(), 0);
    voice.request("V04");
    voice.request("V07", "guard1");
    EXPECT_EQ(voice.queued(), 2u);
    voice.request("V13");
    EXPECT_EQ(voice.current()->id, "V13");
    voice.update(2);
    EXPECT_EQ(voice.current()->id, "V04");
    voice.update(2);
    EXPECT_EQ(voice.current()->id, "V07");
    voice.update(2);
    EXPECT_EQ(voice.current(), nullptr);  // Interrupted V02/V03 were discarded.
}
TEST(VoiceDirector, EqualPriorityQueuesAndPanicInterruptsPanic) {
    VoiceDirector voice(lines(), .25f);
    voice.request("V03");
    voice.request("V07", "guard1");
    EXPECT_EQ(voice.current()->id, "V03");
    voice.request("V13");
    voice.request("V08", "guard1");
    EXPECT_EQ(voice.current()->id, "V08");
    voice.update(2);
    EXPECT_EQ(voice.current()->id, "V07");
}
TEST(VoiceDirector, PauseFreezesSubtitleAndInvalidDeltasDoNotAdvance) {
    VoiceDirector voice(lines(), .25f);
    voice.request("V03");
    voice.update(.5f);
    const auto serial = voice.serial();
    voice.setPaused(true);
    voice.update(100);
    EXPECT_FLOAT_EQ(voice.age(), .5f);
    EXPECT_EQ(voice.serial(), serial);
    voice.setPaused(false);
    voice.update(-1);
    voice.update(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(voice.age(), .5f);
    voice.update(2.5f);
    EXPECT_EQ(voice.current(), nullptr);
}
TEST(VoiceDirector, OccurrenceTrackingPersistsClearAndResetsOnlyForFreshRun) {
    VoiceDirector voice(lines(), .25f);
    EXPECT_TRUE(voice.request("V02"));
    voice.clear();
    EXPECT_FALSE(voice.request("V02"));
    EXPECT_TRUE(voice.request("V07", "guard1"));
    EXPECT_FALSE(voice.request("V07", "guard1"));
    EXPECT_TRUE(voice.request("V07", "guard2"));
    EXPECT_FALSE(voice.request("invalid"));
    voice.resetRun();
    EXPECT_TRUE(voice.request("V02"));
}
TEST(VoiceDirector, LowHealthUsesConfiguredBoundaryOnceAndNeverAfterDeath) {
    VoiceDirector voice(lines(), .25f);
    voice.observeHealth(26, 100);
    EXPECT_EQ(voice.current(), nullptr);
    voice.observeHealth(25, 100);
    ASSERT_NE(voice.current(), nullptr);
    EXPECT_EQ(voice.current()->id, "V24");
    voice.clear();
    voice.observeHealth(10, 100);
    EXPECT_EQ(voice.current(), nullptr);
    voice.resetRun();
    voice.observeHealth(0, 100);
    EXPECT_EQ(voice.current(), nullptr);
    voice.observeHealth(1, 0);
    EXPECT_EQ(voice.current(), nullptr);
}
TEST(VoiceDirector, PlaybackSuppliedDurationAndPagerEventsDriveScheduling) {
    VoiceDirector voice(lines(), .25f);
    voice.setDuration("V07", 4);
    voice.setDuration("V07", -1);
    EventBus events;
    voice.listen(events);
    events.publish(PagerRang{"guard1"});
    events.dispatch();
    ASSERT_NE(voice.current(), nullptr);
    voice.update(3);
    EXPECT_EQ(voice.current()->id, "V07");
    events.publish(PagerMissed{"guard1"});
    events.dispatch();
    EXPECT_EQ(voice.current()->id, "V08");
}
TEST(VoiceDirector, ManifestLoadsAllRecordingsAndRejectsDuplicateIdsOrUnsafePaths) {
    std::ostringstream out;
    Logger logger(out, "");
    auto loaded = loadVoiceLines("assets/config/voice_lines.json", logger);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->size(), 25u);
    EXPECT_TRUE(out.str().empty());
    nlohmann::json data;
    std::ifstream("assets/config/voice_lines.json") >> data;
    TestFiles files;
    data[1]["id"] = "V01";
    EXPECT_FALSE(loadVoiceLines(files.write("duplicate.json", data.dump()), logger));
    data[1]["id"] = "V02";
    data[1]["file"] = "../V02.ogg";
    EXPECT_FALSE(loadVoiceLines(files.write("unsafe.json", data.dump()), logger));
    data[1]["file"] = "audio/voice/V02.ogg";
    data[1]["priority"] = 4294967298ULL;
    EXPECT_FALSE(loadVoiceLines(files.write("overflow-priority.json", data.dump()), logger));
    EXPECT_FALSE(loadVoiceLines("missing-voice-manifest.json", logger));
    EXPECT_NE(out.str().find("[WARN]"), std::string::npos);
}

TEST(VoiceDirector, MissingAudioUsesReadingDurationAndRecordedDurationOverridesIt) {
    VoiceDirector voice({{"V02", "", "one two three four five six", "", 1, 0}}, .25f, 3);
    voice.request("V02");
    ASSERT_NE(voice.current(), nullptr);
    EXPECT_FLOAT_EQ(voice.current()->duration, 2);
    voice.update(1.9f);
    EXPECT_NE(voice.current(), nullptr);
    voice.setDuration("V02", 4);
    voice.update(.2f);
    EXPECT_NE(voice.current(), nullptr);
    voice.update(2);
    EXPECT_EQ(voice.current(), nullptr);
}
TEST(Tutorial, StageOneTriggersEachHintOnceAndRetryKeepsOccurrenceHistory) {
    auto script = lines();
    script.push_back({"V05", "", "guard", "", 1, 2});
    VoiceDirector voice(script, .25f);
    EventBus events;
    voice.listen(events);
    voice.setTutorialContext(1, true, "player");
    EXPECT_NE(voice.hint().find("SPACE"), std::string::npos);
    EXPECT_EQ(voice.current()->id, "V03");
    events.publish(NoiseEmitted{{}, 100, NoiseType::Ping, "player"});
    events.dispatch();
    voice.observeTutorial(true, true);
    voice.update(3);
    EXPECT_NE(voice.hint().find("crouch"), std::string::npos);
    voice.update(3);
    EXPECT_NE(voice.hint().find("RIGHT CLICK"), std::string::npos);
    voice.update(3);
    EXPECT_NE(voice.hint().find("service door"), std::string::npos);
    events.publish(PagerRang{"g1"});
    events.dispatch();
    voice.update(3);
    EXPECT_NE(voice.hint().find("pager"), std::string::npos);
    voice.update(3);
    EXPECT_TRUE(voice.hint().empty());
    voice.clear();
    voice.setTutorialContext(1, true, "player");
    voice.observeTutorial(true, true);
    EXPECT_TRUE(voice.hint().empty());
    voice.resetRun();
    voice.setTutorialContext(1, true, "player");
    EXPECT_FALSE(voice.hint().empty());
}
TEST(Tutorial, HintsOffRemovesTutorialVoiceButPreservesPanicAndLaterPagerSpeech) {
    VoiceDirector voice(lines(), .25f);
    voice.request("V13");
    voice.setTutorialContext(1, true, "player");
    EXPECT_EQ(voice.queued(), 1u);
    voice.setTutorialContext(1, false, "player");
    EXPECT_TRUE(voice.hint().empty());
    EXPECT_EQ(voice.queued(), 0u);
    EXPECT_EQ(voice.current()->id, "V13");
    EventBus events;
    voice.listen(events);
    events.publish(PagerRang{"g1"});
    events.dispatch();
    EXPECT_EQ(voice.queued(), 0u);
    voice.setTutorialContext(3, false, "player");
    events.publish(PagerRang{"g2"});
    events.dispatch();
    EXPECT_EQ(voice.queued(), 1u);
}
TEST(Tutorial, NonPlayerPingsAndLateStagesDoNotTeachAndPauseFreezesToast) {
    VoiceDirector voice(lines(), .25f);
    EventBus events;
    voice.listen(events);
    voice.setTutorialContext(2, true, "player");
    events.publish(NoiseEmitted{{}, 100, NoiseType::Ping, "guard"});
    events.dispatch();
    EXPECT_TRUE(voice.hint().empty());
    events.publish(NoiseEmitted{{}, 100, NoiseType::Ping, "player"});
    events.dispatch();
    voice.update(1);
    voice.setPaused(true);
    voice.update(99);
    EXPECT_FLOAT_EQ(voice.hintAge(), 1);
    voice.setPaused(false);
    voice.setTutorialContext(3, true, "player");
    voice.observeTutorial(true, true);
    EXPECT_TRUE(voice.hint().empty());
}
