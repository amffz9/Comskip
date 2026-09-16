#include "detection/logo_sampling.h"
#include <gtest/gtest.h>

TEST(LogoSampling, PreservesWholeFrameIntervalsAndSamplesFractionalRatesEveryFrame) {
    using comskip::detection::logo_sampling_interval;
    EXPECT_EQ(logo_sampling_interval(25, 1), 25);
    EXPECT_EQ(logo_sampling_interval(29.97, 1), 29);
    EXPECT_EQ(logo_sampling_interval(0.5, 1), 1);
    EXPECT_EQ(logo_sampling_interval(25, 0.01), 1);
    EXPECT_EQ(logo_sampling_interval(std::numeric_limits<int>::max(), 1), std::numeric_limits<int>::max());
}
TEST(LogoSampling, RejectsUnrepresentableAndNonpositiveIntervalsBeforeConversion) {
    using comskip::detection::logo_sampling_interval;
    for (double rate : {0.0, -1.0, 1e20, std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()})
        EXPECT_THROW(logo_sampling_interval(rate, 1), std::invalid_argument);
    for (double seconds : {0.0, -1.0, 1e20, std::numeric_limits<double>::infinity()})
        EXPECT_THROW(logo_sampling_interval(25, seconds), std::invalid_argument);
}
