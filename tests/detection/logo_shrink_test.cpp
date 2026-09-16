#include "recording_context.h"
#include "detection/logo_shrink.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>

bool ProcessLogoTest(RecordingContext&, int, int, int);

namespace {
TEST(LogoShrink, TruncatesAppliedOffsetsAndPreservesFractionalTailComparison) {
    const auto shrink = comskip::detection::logo_shrink(0.5, 1, 25);
    EXPECT_EQ(shrink.head, 12); EXPECT_EQ(shrink.tail, 25);
    const auto closed = comskip::detection::close_logo_block(100, 1000, 25, 1000, shrink);
    ASSERT_TRUE(closed.retained);
    EXPECT_EQ(closed.start, 112); EXPECT_EQ(closed.end, 938);
    EXPECT_EQ(closed.frames_with_logo, 901);
    const auto fractional = comskip::detection::logo_shrink(0.5, 1, 29.97);
    EXPECT_FALSE(comskip::detection::close_logo_block(0, 58, 1, 100, fractional).retained);
    EXPECT_TRUE(comskip::detection::close_logo_block(0, 59, 1, 100, fractional).retained);
}
TEST(LogoShrink, RejectsExtremeOffsetsAndComputesCombinedAmountsWithoutOverflow) {
    using comskip::detection::logo_shrink;
    for (double seconds : {-1.0, 1e20, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
        EXPECT_THROW(logo_shrink(seconds, 0, 25), std::invalid_argument);
    EXPECT_THROW(logo_shrink(0, std::numeric_limits<int>::max(), 25), std::invalid_argument);
    const auto maximum = logo_shrink(std::numeric_limits<int>::max(), 0, 1);
    EXPECT_FALSE(comskip::detection::close_logo_block(0, std::numeric_limits<int>::max(),
        1, 0, maximum).retained); // Doubling the offset exceeds int but is safely compared in a wide type.
    EXPECT_THROW(comskip::detection::close_logo_block(0, 100, 25,
        std::numeric_limits<int>::min(), logo_shrink(0, 0, 25)), std::out_of_range);
}
TEST(LogoShrink, LiveWindowPreservesFractionalBoundsAndClampsToActualStorage) {
    const auto ordinary = comskip::detection::logo_scan_window(100, 1000, 1000, 0.5);
    EXPECT_EQ(ordinary.begin, 99); EXPECT_EQ(ordinary.end, 101);
    const auto bounded = comskip::detection::logo_scan_window(100, 1000, 100, 0.5);
    EXPECT_EQ(bounded.begin, 99); EXPECT_EQ(bounded.end, 100);
    const auto wide = comskip::detection::logo_scan_window(std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    EXPECT_EQ(wide.begin, 1); EXPECT_EQ(wide.end, std::numeric_limits<int>::max());
    EXPECT_THROW(comskip::detection::logo_scan_window(100, 1000, 1000,
        std::numeric_limits<double>::infinity()), std::invalid_argument);
}

std::unique_ptr<RecordingContext> active_logo() {
    auto context = std::make_unique<RecordingContext>();
    context->settings.verbose = 0; context->settings.fps = 25;
    context->settings.logo_filter = 0; context->settings.shrink_logo = 0.5;
    context->settings.shrink_logo_tail = 1;
    context->state.logoFreq = 1; context->state.lastLogoTest = true;
    context->state.logoTrendCounter = 7;
    context->state.logo_block.resize(1); context->state.logo_block[0] = {100, 777};
    context->state.frames_with_logo = 1000;
    context->state.framearray = true;
    context->state.frame.resize(1001); context->state.framenum_real = 1001;
    for (int frame = 1; frame <= 1000; ++frame) {
        context->state.frame[frame].logo_present = true;
        context->state.frame[frame].pts = (frame - 1) / 25.0;
    }
    return context;
}
TEST(LogoShrinkApplication, OrdinaryClosePublishesExactOffsetsAndClearsRemovedTailFrames) {
    auto context = active_logo();
    EXPECT_NO_THROW(ProcessLogoTest(*context, 1000, 0, 1));
    EXPECT_EQ(context->state.logo_block_count, 1);
    EXPECT_EQ(context->state.logo_block[0].start, 112);
    EXPECT_EQ(context->state.logo_block[0].end, 938);
    EXPECT_EQ(context->state.frames_with_logo, 901);
    EXPECT_FALSE(context->state.lastLogoTest); EXPECT_EQ(context->state.logoTrendCounter, 0);
    EXPECT_TRUE(context->state.frame[937].logo_present);
    for (int frame = 938; frame < 1000; ++frame) EXPECT_FALSE(context->state.frame[frame].logo_present);
    EXPECT_TRUE(context->state.frame[1000].logo_present);
}
TEST(LogoShrinkApplication, ExtremeFiniteOffsetsRejectBeforeChangingActiveBlockOrHistory) {
    for (bool tail : {false, true}) {
        auto context = active_logo();
        if (tail) context->settings.shrink_logo_tail = std::numeric_limits<int>::max();
        else context->settings.shrink_logo = 1e20;
        EXPECT_THROW(ProcessLogoTest(*context, 1000, 0, 1), std::invalid_argument);
        EXPECT_EQ(context->state.logo_block_count, 0);
        EXPECT_EQ(context->state.logo_block[0].start, 100); EXPECT_EQ(context->state.logo_block[0].end, 777);
        EXPECT_EQ(context->state.frames_with_logo, 1000);
        EXPECT_TRUE(context->state.lastLogoTest); EXPECT_EQ(context->state.logoTrendCounter, 7);
        EXPECT_TRUE(context->state.frame[938].logo_present);
    }
}
TEST(LogoShrinkApplication, UnrepresentableCounterRejectsBeforePublishingClosure) {
    auto context = active_logo();
    context->state.frames_with_logo = std::numeric_limits<int>::min();
    EXPECT_THROW(ProcessLogoTest(*context, 1000, 0, 1), std::out_of_range);
    EXPECT_EQ(context->state.logo_block[0].start, 100); EXPECT_EQ(context->state.logo_block[0].end, 777);
    EXPECT_TRUE(context->state.lastLogoTest); EXPECT_EQ(context->state.logoTrendCounter, 7);
    EXPECT_EQ(context->state.frames_with_logo, std::numeric_limits<int>::min());
}
TEST(LogoShrinkApplication, InvalidLiveOffsetRejectsBeforeRecordingOrOutputMutation) {
    auto context = active_logo();
    context->settings.shrink_logo = 1e20;
    context->state.black.resize(1); context->state.black_count = 1;
    EXPECT_THROW(BuildCommListAsYouGo(*context), std::invalid_argument);
    EXPECT_EQ(context->state.lastFrameCommCalculated, 0);
    EXPECT_TRUE(context->state.commercial.empty()); EXPECT_EQ(context->state.commercial_count, -1);
    EXPECT_FALSE(context->state.live_file); EXPECT_FALSE(context->state.incommercial_file);
}
TEST(LogoShrinkApplication, ClosureBeyondOwnedFrameStorageRejectsBeforeClearingTail) {
    auto context = active_logo();
    context->state.frame.resize(10);
    EXPECT_THROW(ProcessLogoTest(*context, 1000, 0, 1), std::out_of_range);
    EXPECT_EQ(context->state.logo_block[0].start, 100); EXPECT_EQ(context->state.logo_block[0].end, 777);
    EXPECT_TRUE(context->state.lastLogoTest); EXPECT_EQ(context->state.logoTrendCounter, 7);
}
TEST(LogoCounter, WideHistoryAndSingleFrameAddRejectUnrepresentableCounts) {
    EXPECT_EQ(comskip::detection::add_logo_frames(99, 1), 100);
    EXPECT_THROW(comskip::detection::add_logo_frames(std::numeric_limits<int>::max(), 1), std::out_of_range);
    EXPECT_THROW(comskip::detection::start_logo_block(1000, std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max(), 0), std::out_of_range);
}
TEST(LogoCounterApplication, AppearanceFillsRetrospectiveFramesAndAddsExactHistory) {
    auto context = active_logo();
    context->state.lastLogoTest = false; context->state.logoTrendCounter = 9;
    context->state.minHitsForTrend = 10; context->state.frames_with_logo = 100;
    for (auto& frame : context->state.frame) frame.logo_present = false;
    EXPECT_NO_THROW(ProcessLogoTest(*context, 1000, 1, 1));
    EXPECT_TRUE(context->state.lastLogoTest); EXPECT_EQ(context->state.logoTrendCounter, 0);
    EXPECT_EQ(context->state.logo_block[0].start, 775);
    EXPECT_EQ(context->state.frames_with_logo, 325);
    EXPECT_FALSE(context->state.frame[774].logo_present);
    for (int i = 775; i < 1000; ++i) EXPECT_TRUE(context->state.frame[i].logo_present);
    EXPECT_FALSE(context->state.frame[1000].logo_present);
}
TEST(LogoCounterApplication, AppearanceOverflowRejectsBeforePublishingTrendOrBlock) {
    for (bool trend_overflow : {false, true}) {
        auto context = active_logo();
        context->state.lastLogoTest = false;
        context->state.logoTrendCounter = trend_overflow ? std::numeric_limits<int>::max() : 9;
        context->state.minHitsForTrend = 10;
        context->state.frames_with_logo = std::numeric_limits<int>::max();
        EXPECT_THROW(ProcessLogoTest(*context, 1000, 1, 1), std::out_of_range);
        EXPECT_FALSE(context->state.lastLogoTest);
        EXPECT_EQ(context->state.logoTrendCounter, trend_overflow ? std::numeric_limits<int>::max() : 9);
        EXPECT_EQ(context->state.logo_block[0].start, 100);
        EXPECT_EQ(context->state.frames_with_logo, std::numeric_limits<int>::max());
    }
}
}
