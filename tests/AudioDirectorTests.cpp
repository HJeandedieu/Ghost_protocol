#include <gtest/gtest.h>

#include <limits>

#include "core/EventBus.h"
#include "systems/AudioDirector.h"
TEST(AudioDirector, AlarmCrossfadesOnceOverConfiguredRealTime) {
    EventBus events;
    AudioDirector audio({1, .4f});
    audio.setScene(AudioScene::Stealth);
    audio.listen(events);
    EXPECT_FLOAT_EQ(audio.musicWeights()[1], 1);
    events.publish(AlarmTriggered{});
    events.dispatch();
    audio.update(.25f, false);
    EXPECT_FLOAT_EQ(audio.musicWeights()[1], .75f);
    EXPECT_FLOAT_EQ(audio.musicWeights()[2], .25f);
    events.publish(AlarmTriggered{});
    events.dispatch();
    audio.update(.25f, false);
    EXPECT_FLOAT_EQ(audio.musicWeights()[1], .5f);
    EXPECT_FLOAT_EQ(audio.musicWeights()[2], .5f);
    audio.update(.5f, false);
    EXPECT_FLOAT_EQ(audio.musicWeights()[1], 0);
    EXPECT_FLOAT_EQ(audio.musicWeights()[2], 1);
    EXPECT_EQ(audio.takeSounds(), std::vector<std::string>{"alarm_siren"});
    audio.setScene(AudioScene::Payout);
    EXPECT_FLOAT_EQ(audio.musicWeights()[3], 1);
    audio.setScene(AudioScene::Menu);
    EXPECT_FLOAT_EQ(audio.musicWeights()[0], 1);
    audio.setScene(AudioScene::Silent);
    for (float gain : audio.musicWeights()) EXPECT_FLOAT_EQ(gain, 0);
}
TEST(AudioDirector, DuckingMultipliesMusicWithoutDoubleApplyingMasterOrRaisingMute) {
    AudioDirector audio({1, .4f});
    audio.setScene(AudioScene::Stealth);
    audio.setVolumes(.8f, .7f, .6f, .5f);
    audio.update(.1f, true);
    EXPECT_NEAR(audio.musicGains()[1], .28f, 1e-6f);
    EXPECT_FLOAT_EQ(audio.masterGain(), .8f);
    EXPECT_FLOAT_EQ(audio.sfxGain(), .6f);
    EXPECT_FLOAT_EQ(audio.voiceGain(), .5f);
    audio.update(.1f, false);
    EXPECT_FLOAT_EQ(audio.musicGains()[1], .7f);
    audio.setVolumes(0, 0, 0, 0);
    audio.update(.1f, true);
    EXPECT_FLOAT_EQ(audio.musicGains()[1], 0);
    audio.update(.1f, false);
    EXPECT_FLOAT_EQ(audio.musicGains()[1], 0);
}
TEST(AudioDirector, InvalidDeltaAndVolumesRemainFiniteAndDoNotAdvanceFade) {
    EventBus events;
    AudioDirector audio({0, std::numeric_limits<float>::quiet_NaN()});
    audio.setScene(AudioScene::Stealth);
    audio.listen(events);
    events.publish(AlarmTriggered{});
    events.dispatch();
    audio.update(-1, false);
    audio.update(std::numeric_limits<float>::infinity(), false);
    EXPECT_FLOAT_EQ(audio.musicWeights()[1], 1);
    EXPECT_FLOAT_EQ(audio.musicWeights()[2], 0);
    audio.setVolumes(-1, 2, std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(audio.masterGain(), 0);
    EXPECT_FLOAT_EQ(audio.sfxGain(), 0);
    EXPECT_FLOAT_EQ(audio.voiceGain(), 0);
    audio.update(.5f, true);
    EXPECT_NEAR(audio.musicGains()[1], .2f, 1e-6f);
}
TEST(AudioDirector, EventsRequestSoundsOnceAndLoopRequestsAreDeduplicated) {
    EventBus events;
    AudioDirector audio({});
    audio.setScene(AudioScene::Stealth);
    audio.listen(events);
    audio.bindInteraction("21:44", "door_open");
    events.publish(ShotFired{"Ghost", "whisper", {}, {}});
    events.publish(ShotFired{"Ghost", "gavel", {}, {}});
    events.publish(ShotFired{"Ghost", "chatter", {}, {}});
    events.publish(EntityDamaged{"Ghost", 10, "cop"});
    events.publish(EntityDamaged{"G01", 10, "Ghost"});
    events.publish(EntityDamaged{"G01", 0, "Ghost"});
    events.publish(GuardTakenDown{"G01", true});
    events.publish(PagerRang{"G01"});
    events.publish(InteractionDone{"21:44"});
    events.publish(InteractionDone{"unknown"});
    events.dispatch();
    EXPECT_EQ(audio.takeSounds(), (std::vector<std::string>{
                                      "shot_pistol_supp", "shot_shotgun", "shot_smg", "hit_player",
                                      "hit_enemy", "takedown", "pager_ring", "door_open"}));
    events.dispatch();
    EXPECT_TRUE(audio.takeSounds().empty());
    audio.loop("step_walk");
    audio.loop("step_walk");
    EXPECT_EQ(audio.loops().size(), 1u);
    audio.clearLoops();
    EXPECT_TRUE(audio.loops().empty());
    audio.request("ui_select");
    audio.setScene(AudioScene::Menu);
    EXPECT_TRUE(audio.takeSounds().empty());
}
