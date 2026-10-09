#include <gtest/gtest.h>

#include "core/PointerCapture.h"

TEST(PointerCapture, NativeAcquisitionDiscardsEntryInputAndMenusReleaseCapture) {
    PointerCapture capture;
    auto action = capture.sync(true, true, false, false);
    EXPECT_TRUE(action.request);
    EXPECT_TRUE(action.discardInput);
    EXPECT_FALSE(capture.active());
    action = capture.sync(true, true, true, false);
    EXPECT_TRUE(action.discardInput);
    EXPECT_TRUE(capture.active());
    EXPECT_FALSE(capture.sync(true, true, true, false).discardInput);
    action = capture.sync(false, true, true, false);
    EXPECT_TRUE(action.release);
    EXPECT_TRUE(action.discardInput);
    EXPECT_FALSE(capture.active());
    EXPECT_FALSE(capture.wantsCapture());
}

TEST(PointerCapture, BrowserWaitsForAGestureAndLockLossPausesWithoutReacquiring) {
    PointerCapture capture;
    auto action = capture.sync(true, true, false, true);
    EXPECT_FALSE(action.request);
    EXPECT_FALSE(action.pause);
    EXPECT_TRUE(capture.wantsCapture());
    capture.sync(true, true, true, true);
    action = capture.sync(true, true, false, true);
    EXPECT_TRUE(action.pause);
    EXPECT_TRUE(action.discardInput);
    EXPECT_FALSE(action.request);
    EXPECT_FALSE(capture.active());
    capture.sync(false, true, false, true);
    action = capture.sync(true, true, false, true);
    EXPECT_FALSE(action.pause);
    EXPECT_FALSE(action.request);
    EXPECT_FALSE(capture.active());
    EXPECT_TRUE(capture.sync(true, true, true, true).discardInput);
}

TEST(PointerCapture, FocusLossPausesEvenBeforeAcquisitionAndNeverRequestsWhileUnfocused) {
    PointerCapture capture;
    auto action = capture.sync(true, false, false, false);
    EXPECT_TRUE(action.pause);
    EXPECT_TRUE(action.discardInput);
    EXPECT_FALSE(action.request);
    EXPECT_FALSE(capture.wantsCapture());
    capture.sync(true, true, true, false);
    action = capture.sync(true, false, true, false);
    EXPECT_TRUE(action.release);
    EXPECT_TRUE(action.pause);
    EXPECT_FALSE(capture.active());
}

TEST(PointerCapture, PausedFocusChangesDoNotPauseOrCaptureAgain) {
    PointerCapture capture;
    for (const bool focused : {false, true, false, true}) {
        const auto action = capture.sync(false, focused, false, true);
        EXPECT_FALSE(action.pause);
        EXPECT_FALSE(action.request);
        EXPECT_FALSE(capture.wantsCapture());
    }
}
