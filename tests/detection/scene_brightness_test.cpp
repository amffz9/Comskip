#include "scene_brightness.h"

#include <gtest/gtest.h>

namespace {

TEST(SceneBrightness, ReturnsAnIndexWithinTheHistogram)
{
    const auto index = comskip::detection::brightness_histogram_index(255, 256);
    ASSERT_TRUE(index);
    EXPECT_EQ(*index, 255U);
}

TEST(SceneBrightness, RejectsEitherSideOfTheHistogram)
{
    const auto negative = comskip::detection::brightness_histogram_index(-1, 256);
    const auto upper_bound = comskip::detection::brightness_histogram_index(256, 256);
    ASSERT_FALSE(negative);
    ASSERT_FALSE(upper_bound);
    EXPECT_EQ(negative.error(), comskip::detection::BrightnessIndexError::out_of_range);
    EXPECT_EQ(upper_bound.error(), comskip::detection::BrightnessIndexError::out_of_range);
}

TEST(SceneBrightness, RejectsEveryValueWhenTheHistogramIsEmpty)
{
    EXPECT_FALSE(comskip::detection::brightness_histogram_index(0, 0));
}

} // namespace
