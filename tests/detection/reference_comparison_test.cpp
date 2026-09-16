#include "detection/reference_comparison.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <stdexcept>

namespace {
using namespace comskip::detection;
using comskip::output::CommercialInterval;
using comskip::output::FrameIndex;
FrameIndex length(const std::vector<ReferenceComparisonEvent>& events, ReferenceEventKind kind) {
    FrameIndex result = 0;
    for (const auto& event : events) if (event.kind == kind) result += event.interval.end_frame - event.interval.start_frame;
    return result;
}
TEST(ReferenceComparison, EmptyListsDoNotNeedSentinelsAndScoreAllExtrasOrMisses) {
    const std::array intervals{CommercialInterval{20, 80}, CommercialInterval{100, 160}};
    EXPECT_TRUE(compare_reference_intervals({}, {}).empty());
    const auto missed = compare_reference_intervals(intervals, {});
    EXPECT_EQ(length(missed, ReferenceEventKind::false_negative), 120);
    EXPECT_EQ(length(missed, ReferenceEventKind::reference_duration), 120);
    EXPECT_EQ(length(missed, ReferenceEventKind::false_positive), 0);
    const auto extra = compare_reference_intervals({}, intervals);
    EXPECT_EQ(length(extra, ReferenceEventKind::false_positive), 120);
    EXPECT_EQ(length(extra, ReferenceEventKind::false_negative), 0);
}
TEST(ReferenceComparison, PreservesBoundaryToleranceAndMixedErrorScores) {
    const std::array reference{CommercialInterval{100, 200}, CommercialInterval{300, 400}};
    const std::array nearby{CommercialInterval{139, 239}, CommercialInterval{339, 439}};
    const auto tolerated = compare_reference_intervals(reference, nearby);
    EXPECT_EQ(length(tolerated, ReferenceEventKind::false_positive), 0);
    EXPECT_EQ(length(tolerated, ReferenceEventKind::false_negative), 0);
    EXPECT_EQ(length(tolerated, ReferenceEventKind::reference_duration), 200);
    const std::array changed{CommercialInterval{150, 250}, CommercialInterval{300, 460}};
    const auto errors = compare_reference_intervals(reference, changed);
    EXPECT_EQ(length(errors, ReferenceEventKind::false_negative), 50);
    EXPECT_EQ(length(errors, ReferenceEventKind::false_positive), 110);
}
TEST(ReferenceComparison, FullCapacityNeverChangesRealIntervalsOrWritesAdditionalSlots) {
    std::vector<CommercialInterval> intervals;
    for (FrameIndex i = 0; i < 100000; ++i) intervals.push_back({i * 4, i * 4 + 3});
    const auto events = compare_reference_intervals(intervals, intervals);
    EXPECT_EQ(length(events, ReferenceEventKind::reference_duration), 300000);
    EXPECT_EQ(length(events, ReferenceEventKind::false_positive), 0);
    EXPECT_EQ(intervals.back().start_frame, 399996); EXPECT_EQ(intervals.back().end_frame, 399999);
}
TEST(ReferenceComparison, RejectsInvalidIntervalsAndHandlesExtremeValidValuesWithoutOverflow) {
    const std::array invalid{CommercialInterval{10, 20}, CommercialInterval{19, 30}};
    EXPECT_THROW(compare_reference_intervals(invalid, {}), std::invalid_argument);
    const std::array reversed{CommercialInterval{10, 9}};
    EXPECT_THROW(compare_reference_intervals({}, reversed), std::invalid_argument);
    EXPECT_THROW(compare_reference_intervals({}, {}, -1), std::invalid_argument);
    const auto maximum = std::numeric_limits<FrameIndex>::max();
    const std::array extreme{CommercialInterval{maximum - 100, maximum}};
    EXPECT_EQ(length(compare_reference_intervals({}, extreme), ReferenceEventKind::false_positive), 100);
}
}
