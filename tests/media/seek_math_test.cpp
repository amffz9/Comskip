#include "media/seek_math.h"
#include <gtest/gtest.h>
#include <limits>
using namespace comskip::media;
TEST(SeekMath, UnknownDurationUsesFramesDividedByRate) {
    ASSERT_TRUE(recording_duration(250,25)); EXPECT_DOUBLE_EQ(*recording_duration(250,25),10);
    EXPECT_FALSE(recording_duration(0,25)); EXPECT_FALSE(recording_duration(250,0));
    EXPECT_FALSE(recording_duration(250,std::numeric_limits<double>::infinity()));
}
TEST(SeekMath, BytePositionRejectsIoErrorsAndInvalidInputs) {
    EXPECT_FALSE(byte_seek_position(-5,10,2));
    EXPECT_FALSE(byte_seek_position(100,0,2));
    EXPECT_FALSE(byte_seek_position(100,10,std::numeric_limits<double>::quiet_NaN()));
    EXPECT_EQ(*byte_seek_position(1000,10,2.5),250);
    EXPECT_EQ(*byte_seek_position(1000,10,-2),0);
    EXPECT_EQ(*byte_seek_position(1000,10,20),1000);
}
TEST(SeekMath, TimestampConversionChecksFiniteRangeAndTimeBase) {
    EXPECT_EQ(*timestamp_seek_position(2,0,1,25),50);
    EXPECT_EQ(*timestamp_seek_position(-2,0,1,25),0);
    EXPECT_FALSE(timestamp_seek_position(std::numeric_limits<double>::infinity(),0,1,25));
    EXPECT_FALSE(timestamp_seek_position(1,0,0,25));
    EXPECT_FALSE(timestamp_seek_position(std::numeric_limits<double>::max(),0,1,25));
}
TEST(SeekMath, TimestampStartAdditionIsChecked) {
    EXPECT_EQ(*timestamp_seek_position(2,0,1,25,100),150);
    EXPECT_FALSE(timestamp_seek_position(1,0,1,1,std::numeric_limits<std::int64_t>::max()));
    EXPECT_EQ(*timestamp_seek_position(0,0,1,1,-100),-100);
}
