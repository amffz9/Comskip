#include "ui/review_intervals.h"
#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace {
struct Interval { long start_frame, end_frame; };
using comskip::ui::IntervalDirection;
auto boundary(std::span<const Interval> values, int last, long frame, IntervalDirection direction) {
    return comskip::ui::review_interval_boundary(values, last, frame, direction);
}
}
TEST(ReviewIntervals, EmptyAndOutsidePositionsHaveNoBoundary) {
    const std::array<Interval, 2> values{{{10, 20}, {30, 40}}};
    EXPECT_FALSE(boundary({}, -1, 0, IntervalDirection::next));
    EXPECT_FALSE(boundary({}, -1, 0, IntervalDirection::previous));
    EXPECT_FALSE(boundary(values, 1, 5, IntervalDirection::previous));
    EXPECT_FALSE(boundary(values, 1, 40, IntervalDirection::next));
    EXPECT_FALSE(boundary(values, -1, 20, IntervalDirection::next));
}
TEST(ReviewIntervals, PreservesFiveFrameSkipAtBothBoundaries) {
    const std::array<Interval, 2> values{{{10, 20}, {30, 40}}};
    EXPECT_EQ(boundary(values, 1, 15, IntervalDirection::next), 20);
    EXPECT_EQ(boundary(values, 1, 16, IntervalDirection::next), 40);
    EXPECT_EQ(boundary(values, 1, 35, IntervalDirection::previous), 30);
    EXPECT_EQ(boundary(values, 1, 34, IntervalDirection::previous), 10);
}
TEST(ReviewIntervals, FullCapacityHasNoSentinelRequirement) {
    std::vector<Interval> values(100000);
    for (long i = 0; i < 100000; ++i) values[i] = {i * 30 + 1, i * 30 + 20};
    EXPECT_EQ(boundary(values, 99999, 2999980, IntervalDirection::next), values.back().end_frame);
    EXPECT_FALSE(boundary(values, 99999, values.back().end_frame, IntervalDirection::next));
    EXPECT_FALSE(boundary(values, 99999, 0, IntervalDirection::previous));
}
TEST(ReviewIntervals, RejectsInvalidCountsBeforeReadingStorage) {
    EXPECT_THROW(boundary({}, 0, 0, IntervalDirection::next), std::out_of_range);
    EXPECT_THROW(boundary({}, -2, 0, IntervalDirection::previous), std::out_of_range);
}
TEST(ReviewIntervals, ExtremeCursorPositionsCannotOverflowTheSkipMargin) {
    const std::array<Interval, 1> values{{{10, 20}}};
    EXPECT_FALSE(boundary(values, 0, std::numeric_limits<long>::max(), IntervalDirection::next));
    EXPECT_FALSE(boundary(values, 0, std::numeric_limits<long>::min(), IntervalDirection::previous));
    EXPECT_EQ(boundary(values, 0, std::numeric_limits<long>::min(), IntervalDirection::next), 20);
    EXPECT_EQ(boundary(values, 0, std::numeric_limits<long>::max(), IntervalDirection::previous), 10);
}
