#include "logo_histogram.h"

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

using namespace comskip::detection;

namespace {
std::vector<frame_info> frames(std::initializer_list<double> values)
{
    std::vector<frame_info> result(values.size() + 1);
    std::size_t index = 1;
    for (const auto value : values) result[index++].currentGoodEdge = value;
    return result;
}
}

TEST(LogoHistogram, MapsBoundaryValuesAndPreservesLegacyQualitySelection)
{
    const auto input = frames({0.0, 0.5, 1.0});
    const auto result = build_logo_histogram(input, input.size(), 20);
    ASSERT_TRUE(result);
    ASSERT_EQ(result->counts.size(), 20u);
    EXPECT_EQ(result->counts[0], 1u);
    EXPECT_EQ(result->counts[9], 1u);
    EXPECT_EQ(result->counts[19], 1u);
    EXPECT_EQ(result->denominator, 4u); // Legacy calculation includes frame zero.
    EXPECT_DOUBLE_EQ(result->quality, 15.5 / 20.0);
}

TEST(LogoHistogram, RejectsEveryInvalidValueBeforeProducingAResult)
{
    for (const auto invalid : {-0.001, std::nextafter(1.0, 2.0),
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
        const auto input = frames({0.25, invalid});
        const auto result = build_logo_histogram(input, input.size(), 20);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), LogoHistogramError::invalid_edge_value);
    }
}

TEST(LogoHistogram, RejectsInvalidCountsBucketsAndEmptyObservations)
{
    const auto input = frames({0.5});
    EXPECT_EQ(build_logo_histogram(input, input.size() + 1, 20).error(), LogoHistogramError::invalid_frame_count);
    EXPECT_EQ(build_logo_histogram(input, 0, 20).error(), LogoHistogramError::invalid_frame_count);
    EXPECT_EQ(build_logo_histogram(input, input.size(), 1).error(), LogoHistogramError::invalid_bucket_count);
    EXPECT_EQ(build_logo_histogram(input, input.size(), 257).error(), LogoHistogramError::invalid_bucket_count);
    const std::array<frame_info, 1> sentinel{};
    EXPECT_EQ(build_logo_histogram(sentinel, 1, 20).error(), LogoHistogramError::empty_samples);
}

TEST(LogoHistogram, SelectsAtLargeDenominatorsWithoutOverflow)
{
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    const std::array counts{maximum / 2, std::uint64_t{0}};
    const auto result = select_logo_quality(counts, maximum);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.75);
    const std::array overflowing{maximum, std::uint64_t{1}};
    EXPECT_EQ(select_logo_quality(overflowing, maximum).error(), LogoHistogramError::count_overflow);
}
