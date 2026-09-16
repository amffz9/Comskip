#include "media/decode_progress.h"
#include <gtest/gtest.h>
#include <limits>
using namespace comskip::media;
using namespace std::chrono_literals;
TEST(DecodeProgress, ReportsEachDecodedFrameOnceAtOneSecondIntervals) {
    DecodeProgress progress;
    const DecodeProgress::TimePoint start{};
    progress.observe_frame(start);
    progress.observe_frame(start + 500ms);
    EXPECT_FALSE(progress.report(start + 999ms));
    const auto first = progress.report(start + 1s);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->frames, 2);
    EXPECT_DOUBLE_EQ(first->average_fps(), 2);
    EXPECT_DOUBLE_EQ(first->interval_fps(), 2);
    EXPECT_FALSE(progress.report(start + 1s));
    progress.observe_frame(start + 1500ms);
    const auto next = progress.report(start + 2s);
    ASSERT_TRUE(next);
    EXPECT_EQ(next->interval_frames, 1);
    EXPECT_DOUBLE_EQ(next->average_fps(), 1.5);
    EXPECT_DOUBLE_EQ(next->interval_fps(), 1);
}
TEST(DecodeProgress, EmptySummaryResetAndIndependentAnalysesHaveNoSharedClock) {
    DecodeProgress first, second;
    const DecodeProgress::TimePoint start{};
    EXPECT_EQ(first.snapshot(start).frames, 0);
    EXPECT_DOUBLE_EQ(first.snapshot(start).average_fps(), 0);
    first.observe_frame(start);
    EXPECT_DOUBLE_EQ(first.snapshot(start).average_fps(), 0);
    second.observe_frame(start + 100h);
    EXPECT_EQ(second.snapshot(start + 100h).frames, 1);
    first.reset();
    first.observe_frame(start + 1h);
    EXPECT_EQ(first.snapshot(start + 1h).elapsed, 0s);
}
TEST(DecodeProgress, LongAnalysesRetainWideDurationsAndSafePresentation) {
    DecodeProgress progress;
    const DecodeProgress::TimePoint start{};
    progress.observe_frame(start);
    const auto summary = progress.snapshot(start + 10000h);
    EXPECT_EQ(summary.elapsed, 10000h);
    EXPECT_GT(summary.average_fps(), 0);
    EXPECT_EQ(decode_position(61 * 3600 + 62), "61:01:02");
    EXPECT_EQ(decode_completion_percent(50, 100), 50);
    EXPECT_EQ(decode_completion_percent(101, 100), 100);
    EXPECT_EQ(decode_completion_percent(1, 0), 0);
    EXPECT_EQ(decode_completion_percent(1, std::numeric_limits<double>::infinity()), 0);
    EXPECT_EQ(decode_completion_percent(std::numeric_limits<double>::quiet_NaN(), 10), 0);
    EXPECT_EQ(decode_position(std::numeric_limits<double>::infinity()), " 0:00:00");
}
