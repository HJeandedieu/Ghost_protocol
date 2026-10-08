#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

#include "audio/AudioPlayback.h"
#include "core/Logger.h"
#include "systems/AudioDirector.h"

TEST(AudioPlaybackTests, UnavailableDeviceLeavesGameplayRunningAndDrainsRequests) {
    ASSERT_FALSE(IsAudioDeviceReady());
    std::ostringstream output;
    Logger logger(output, "");
    AudioPlayback playback(logger, "missing-audio-directory");
    AudioDirector director({});
    director.setScene(AudioScene::Stealth);
    director.request("ping_small");
    playback.update(director);
    EXPECT_TRUE(director.takeSounds().empty());
    EXPECT_EQ(director.scene(), AudioScene::Stealth);
    EXPECT_FALSE(playback.playVoice("V01"));
    EXPECT_FALSE(playback.voicePlaying());
    EXPECT_NE(output.str().find("Audio device unavailable"), std::string::npos);
}
TEST(AudioPlaybackTests, EveryMusicSfxAndVoiceAssetDecodesToFiniteAudio) {
    int music = 0, sfx = 0, voice = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/audio")) {
        if (entry.path().extension() != ".ogg") continue;
        SCOPED_TRACE(entry.path().string());
        const auto group = entry.path().parent_path().filename().string();
        Wave wave = LoadWave(entry.path().string().c_str());
        ASSERT_TRUE(IsWaveValid(wave));
        EXPECT_GT(wave.frameCount, 0u);
        EXPECT_EQ(wave.channels, group == "music" ? 2u : 1u);
        EXPECT_EQ(wave.sampleRate, group == "voice" ? 24000u : 44100u);
        float* samples = LoadWaveSamples(wave);
        ASSERT_NE(samples, nullptr);
        float peak = 0;
        for (std::size_t i = 0; i < static_cast<std::size_t>(wave.frameCount) * wave.channels;
             ++i) {
            ASSERT_TRUE(std::isfinite(samples[i]));
            peak = std::max(peak, std::abs(samples[i]));
        }
        EXPECT_GT(peak, .001f);
        UnloadWaveSamples(samples);
        UnloadWave(wave);
        if (group == "music") ++music;
        if (group == "sfx") ++sfx;
        if (group == "voice") ++voice;
    }
    EXPECT_EQ(music, 4);
    EXPECT_EQ(sfx, 34);
    EXPECT_EQ(voice, 25);
}
