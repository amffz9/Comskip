#include "histogram_report.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>

using comskip::output::HistogramReportError;
using comskip::output::make_histogram_report;

TEST(HistogramReport, HandlesEmptyCountsAndZeroDenominatorWithoutNonfiniteValues) {
    const std::array<int,3> counts{};
    const auto report=make_histogram_report<int>(counts,counts.size(),counts.size(),10,70,0);
    ASSERT_TRUE(report);
    EXPECT_DOUBLE_EQ(report->divisor,0);
    ASSERT_EQ(report->rows.size(),3u);
    EXPECT_EQ(report->rows[2].label,20);
    EXPECT_DOUBLE_EQ(report->rows[2].cumulative_fraction,0);
    EXPECT_TRUE(report->rows[2].stars.empty());
}
TEST(HistogramReport, PreservesLegacyStarAndCumulativeFormattingInputs) {
    const std::array<int,3> counts{2,4,0};
    const auto report=make_histogram_report<int>(counts,counts.size(),counts.size(),5,10,8);
    ASSERT_TRUE(report);
    EXPECT_DOUBLE_EQ(report->divisor,2.5);
    EXPECT_EQ(report->rows[0].stars,"******");
    EXPECT_EQ(report->rows[1].stars,std::string(11,'*'));
    EXPECT_DOUBLE_EQ(report->rows[1].cumulative_fraction,0.75);
}
TEST(HistogramReport, RejectsNegativeCountsAndInvalidGeometry) {
    const std::array<int,2> negative{1,-1};
    EXPECT_EQ(make_histogram_report<int>(negative,2,2,1,70,1).error(),HistogramReportError::negative_count);
    EXPECT_EQ(make_histogram_report<int>(negative,3,2,1,70,1).error(),HistogramReportError::invalid_geometry);
    EXPECT_EQ(make_histogram_report<int>(negative,1,1,1,0,1).error(),HistogramReportError::invalid_geometry);
}
