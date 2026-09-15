#include "commercial_length.h"
#include <math.h>
#include <stdio.h>

#include <gtest/gtest.h>
#include "settings.h"
#include <fstream>
#include <iterator>

TEST(CommercialLength, PreservesDetectionPolicy)
{
    CommercialLengthPolicy policy = { 25.0, -1.0, 120.0 };
    CommercialLengthMatch match;
    const double rates[] = { 23.976, 25.0, 29.97, 50.0, 59.94 };
    unsigned int i;

    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        policy.fps = rates[i];
        EXPECT_TRUE(commercial_length_match(30.0, 0.5, 1, &policy, &match));
        EXPECT_TRUE(!commercial_length_match(32.0, 0.5, 1, &policy, &match));
    }
    policy.fps = 25.0;
    /* Inclusive tolerance, with the existing whole-frame truncation. */
    EXPECT_TRUE(commercial_length_within_tolerance(30.48, 30.0, 0.5, 25.0));
    EXPECT_TRUE(!commercial_length_within_tolerance(30.52, 30.0, 0.5, 25.0));
    EXPECT_TRUE(commercial_length_within_tolerance(29.52, 30.0, 0.5, 25.0));
    EXPECT_TRUE(!commercial_length_within_tolerance(29.48, 30.0, 0.5, 25.0));

    EXPECT_TRUE(!commercial_length_match(5.0, 0.5, 1, &policy, &match));
    EXPECT_TRUE(commercial_length_match(5.0, 0.5, 0, &policy, &match));
    EXPECT_TRUE(!commercial_length_match(35.0, 0.5, 1, &policy, &match));
    EXPECT_TRUE(commercial_length_match(35.0, 0.5, 0, &policy, &match));
    EXPECT_TRUE(!commercial_length_match(18.0, 0.5, 1, &policy, &match));
    EXPECT_TRUE(!commercial_length_match(72.0, 0.5, 1, &policy, &match));

    EXPECT_TRUE(commercial_length_match(29.89, 0.0, 1, &policy, &match));
    EXPECT_TRUE(fabs(match.adjusted_length - 30.0) < 0.000001);
    EXPECT_TRUE(fabs(match.delta) < 0.000001);
    EXPECT_TRUE(match.tolerance == 0.5);
    EXPECT_TRUE(commercial_length_match(30.7, 9.0, 1, &policy, &match));
    EXPECT_TRUE(match.tolerance == 1.0);
    EXPECT_TRUE(!commercial_length_match(31.2, 9.0, 1, &policy, &match));

    policy.tolerance_override = 0.5;
    EXPECT_TRUE(!commercial_length_match(30.7, 1.0, 1, &policy, &match));
    policy.tolerance_override = 1.0;
    EXPECT_TRUE(commercial_length_match(30.7, 0.5, 1, &policy, &match));

    policy.min_show_segment_length = 33.0;
    EXPECT_TRUE(!commercial_length_match(30.0, 0.5, 1, &policy, &match));
    policy.min_show_segment_length = 33.01;
    EXPECT_TRUE(commercial_length_match(30.0, 0.5, 1, &policy, &match));
    match.adjusted_length = -123.0;
    EXPECT_TRUE(!commercial_length_match(200.0, 0.5, 1, &policy, &match));
    EXPECT_TRUE(match.adjusted_length == -123.0);

}

TEST(CommercialLength, LoadsRegionalProfileWithoutRecompilation) {
    using namespace comskip::config;
    std::ifstream file(std::string(COMSKIP_SOURCE_DIR) + "/config/profiles/china.ini");
    ASSERT_TRUE(file.good());
    std::string text((std::istreambuf_iterator<char>(file)), {});
    apply_settings(Ini(text));
    CommercialLengthPolicy policy{25, -1, 120};
    CommercialLengthMatch match{};
    EXPECT_TRUE(commercial_length_match(18, 0.5, 1, &policy, &match));
    EXPECT_TRUE(commercial_length_match(72, 0.5, 1, &policy, &match));
    apply_settings(defaults());
    EXPECT_FALSE(commercial_length_match(18, 0.5, 1, &policy, &match));
}
