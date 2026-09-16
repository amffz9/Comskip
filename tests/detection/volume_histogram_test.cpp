#include "volume_histogram.h"
#include <gtest/gtest.h>

using comskip::detection::VolumeBucketError;
using comskip::detection::volume_histogram_bucket;

TEST(VolumeHistogram, MapsSupportedBoundaryValues) {
    EXPECT_EQ(volume_histogram_bucket(0,256).value(),0u);
    EXPECT_EQ(volume_histogram_bucket(9,256).value(),0u);
    EXPECT_EQ(volume_histogram_bucket(10,256).value(),1u);
    EXPECT_EQ(volume_histogram_bucket(2559,256).value(),255u);
}
TEST(VolumeHistogram, RejectsNegativeUnrepresentableAndInvalidGeometry) {
    EXPECT_EQ(volume_histogram_bucket(-1,256).error(),VolumeBucketError::negative_volume);
    EXPECT_EQ(volume_histogram_bucket(2560,256).error(),VolumeBucketError::outside_histogram);
    EXPECT_EQ(volume_histogram_bucket(1,0).error(),VolumeBucketError::outside_histogram);
    EXPECT_EQ(volume_histogram_bucket(1,256,0).error(),VolumeBucketError::invalid_width);
}
