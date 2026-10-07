#include <gtest/gtest.h>

#include <limits>
#include <sstream>

#include "core/Logger.h"
#include "render/Renderer.h"
#include "states/PayoutState.h"
class PayoutScreen : public testing::Test {
   protected:
    void SetUp() override {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1280, 720, "Payout test");
    }
    void TearDown() override { CloseWindow(); }
};
TEST_F(PayoutScreen, ReceiptCountsFinalizedAmountThenStampsAndSupportsImmediateNavigation) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    Input input;
    Payout payout;
    payout.subtotal = 200000.25;
    payout.ghostBonus = 50000;
    payout.timeBonus = 10000;
    payout.handlerCut = 39000;
    payout.deathPenalty = 10000;
    payout.finalAmount = 210533.25;
    payout.rank = 'S';
    payout.deductions = {47, 120, 300};
    int again = 0, menu = 0;
    PayoutState state(input, renderer, {}, payout, [&] { ++again; }, [&] { ++menu; });
    EXPECT_EQ(state.visibleLines(), 0);
    EXPECT_DOUBLE_EQ(state.displayedAmount(), 0);
    input.confirmPressed = true;
    state.update(.01f);
    EXPECT_EQ(again, 1);
    input.clearEdges();
    state.update(1.44f);
    EXPECT_EQ(state.visibleLines(), 8);
    EXPECT_GT(state.displayedAmount(), 0);
    EXPECT_NEAR(state.stampProgress(), 0, 1e-6f);
    state.update(1.19f);
    EXPECT_NEAR(state.displayedAmount(), payout.finalAmount, .01);
    EXPECT_NEAR(state.stampProgress(), 0, 1e-6f);
    state.update(.3f);
    EXPECT_FLOAT_EQ(state.stampProgress(), 1);
    renderer.beginFrame();
    state.render(0);
    renderer.present();
    auto image = LoadImageFromTexture(renderer.frameTexture());
    ImageFlipVertical(&image);
    EXPECT_TRUE(ExportImage(image, GP_RENDER_OUTPUT_DIRECTORY "/day24-payout.png"));
    UnloadImage(image);
    input.backPressed = true;
    state.update(.01f);
    EXPECT_EQ(menu, 1);
}
TEST_F(PayoutScreen, ReducedEffectsShowsReceiptImmediatelyAndInvalidDeltaDoesNotAdvance) {
    std::ostringstream output;
    Logger logger(output, "");
    Renderer renderer(logger);
    renderer.setReduceEffects(true);
    Input input;
    Payout payout;
    payout.finalAmount = 0;
    int menu = 0;
    PayoutState state(input, renderer, {}, payout, [] {}, [&] { ++menu; });
    EXPECT_EQ(state.visibleLines(), 5);
    EXPECT_DOUBLE_EQ(state.displayedAmount(), 0);
    state.update(std::numeric_limits<float>::quiet_NaN());
    state.update(-1);
    EXPECT_NEAR(state.stampProgress(), 0, 1e-6f);
    state.update(.25f);
    EXPECT_EQ(state.stampProgress(), 1);
    input.mouseInViewport = true;
    input.mouseLogical = {120, 530};
    input.startClicked = true;
    state.update(.01f);
    EXPECT_EQ(menu, 1);
}
