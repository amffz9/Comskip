#include "scene_sampling.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <stdexcept>

using namespace comskip::detection;
TEST(SceneSampling, BrightPixelScalingPreservesOrdinaryAndNegativeSettings) {
    EXPECT_EQ(scaled_bright_pixel_limit(20,720,480),20);
    EXPECT_EQ(scaled_bright_pixel_limit(10,320,240),2);
    EXPECT_EQ(scaled_bright_pixel_limit(-10,320,240),-2);
    EXPECT_EQ(scaled_bright_pixel_limit(0,320,240),0);
    EXPECT_EQ(scaled_bright_pixel_limit(1,2,2),0);
}
TEST(SceneSampling, BrightPixelScalingHandlesIntegerDomainBoundaries) {
    const auto maximum=std::numeric_limits<int>::max();
    const auto minimum=std::numeric_limits<int>::min();
    EXPECT_EQ(scaled_bright_pixel_limit(maximum,maximum,1),
              static_cast<std::int64_t>(maximum)*maximum/720/480);
    EXPECT_EQ(scaled_bright_pixel_limit(minimum,maximum,1),
              static_cast<std::int64_t>(minimum)*maximum/720/480);
    EXPECT_EQ(scaled_bright_pixel_limit(maximum,320,240),477218588);
    EXPECT_THROW(scaled_bright_pixel_limit(1,maximum,2),std::invalid_argument);
    EXPECT_THROW(scaled_bright_pixel_limit(1,0,240),std::invalid_argument);
    EXPECT_THROW(scaled_bright_pixel_limit(1,320,-1),std::invalid_argument);
}
TEST(SceneSampling, PreservesLegacyNormalizationAndAllowsZeroBorder) {
    auto value = validate_scene_sampling(320,240,352,10);
    EXPECT_EQ(value.storage_size,352u*240);
    EXPECT_EQ(value.initial_brightness_divisor,332u*220/16);
    EXPECT_EQ(value.step,1);
    EXPECT_NO_THROW(validate_scene_sampling(320,240,320,0));
}
TEST(SceneSampling, SmallRetainedAreaHasNonzeroNormalization) {
    EXPECT_EQ(validate_scene_sampling(2,2,2,0).initial_brightness_divisor,1);
    EXPECT_EQ(validate_scene_sampling(320,240,320,119).initial_brightness_divisor,10);
    EXPECT_THROW(validate_scene_sampling(320,240,320,120),std::invalid_argument);
    EXPECT_THROW(validate_scene_sampling(2,2,2,1),std::invalid_argument);
}
TEST(SceneSampling, RejectsInvalidGeometryWithoutSignedOverflow) {
    for (auto g : std::array<std::array<int,4>,8>{std::array{0,240,320,0},
        {-1,240,320,0},{320,0,320,0},{320,-1,320,0},{320,240,319,0},
        {320,240,320,-1},{1,240,320,0},{320,1,320,0}})
        EXPECT_THROW(validate_scene_sampling(g[0],g[1],g[2],g[3]),std::invalid_argument);
    EXPECT_THROW(validate_scene_sampling(320,240,320,std::numeric_limits<int>::max()),
                 std::invalid_argument);
    EXPECT_THROW(validate_scene_sampling(std::numeric_limits<int>::max(),240,
        std::numeric_limits<int>::max(),0),std::invalid_argument);
}
TEST(SceneSampling, BrightnessThresholdsAreBoundedRegardlessOfOrder) {
    for (int bad : {-1,256,std::numeric_limits<int>::max()}) {
        EXPECT_THROW(validate_scene_brightness(bad,5),std::invalid_argument);
        EXPECT_THROW(validate_scene_brightness(20,bad),std::invalid_argument);
    }
    EXPECT_NO_THROW(validate_scene_brightness(0,255));
    EXPECT_NO_THROW(validate_scene_brightness(255,0));
}
