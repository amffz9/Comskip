#include "weighted_scores.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>
using namespace comskip::detection;

TEST(WeightedScores, SelectsByFramesAndPreservesFloorBoundary) {
    const std::array samples{WeightedScore{9, 1}, WeightedScore{2, 8}, WeightedScore{5, 1}};
    EXPECT_EQ(weighted_score_threshold(samples, 0), 2);
    EXPECT_EQ(weighted_score_threshold(samples, .89), 2);
    EXPECT_EQ(weighted_score_threshold(samples, .9), 5);
    EXPECT_EQ(weighted_score_threshold(samples, 1), 9);
    EXPECT_EQ(samples.front().score, 9);
}
TEST(WeightedScores, RejectsInvalidInputsWithoutReadingPastSamples) {
    const std::array valid{WeightedScore{1, 1}};
    EXPECT_EQ(weighted_score_threshold({}, .5).error(), ScoreError::empty);
    for (double percentile : {-1., 1.1, std::numeric_limits<double>::quiet_NaN()})
        EXPECT_EQ(weighted_score_threshold(valid, percentile).error(), ScoreError::invalid_percentile);
    const std::array invalid{WeightedScore{1, 0}};
    EXPECT_EQ(weighted_score_threshold(invalid, .5).error(), ScoreError::invalid_sample);
    const std::array nonfinite{WeightedScore{std::numeric_limits<double>::infinity(), 1}};
    EXPECT_EQ(weighted_score_threshold(nonfinite, .5).error(), ScoreError::invalid_sample);
}
TEST(WeightedScores, RejectsFrameCountOverflow) {
    const std::array samples{WeightedScore{1, std::uint64_t{1} << 63}};
    EXPECT_EQ(weighted_score_threshold(samples, .5).error(), ScoreError::overflow);
}
