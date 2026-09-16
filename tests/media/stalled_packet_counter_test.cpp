#include "media/stalled_packet_counter.h"
#include <gtest/gtest.h>

TEST(StalledPacketCounter, ReportsAtConsecutiveThresholdAndRepeatsPeriodically) {
    comskip::media::StalledPacketCounter counter;
    for (int window = 0; window < 3; ++window) {
        for (int packet = 0; packet < 1000; ++packet) EXPECT_FALSE(counter.observe(false));
        EXPECT_TRUE(counter.observe(false));
    }
}
TEST(StalledPacketCounter, VideoProgressResetsTheConsecutiveWindow) {
    comskip::media::StalledPacketCounter counter;
    for (int packet = 0; packet < 1000; ++packet) EXPECT_FALSE(counter.observe(false));
    EXPECT_FALSE(counter.observe(true));
    for (int packet = 0; packet < 1000; ++packet) EXPECT_FALSE(counter.observe(false));
    EXPECT_TRUE(counter.observe(false));
}
TEST(StalledPacketCounter, IndependentCountersDoNotShareProgress) {
    comskip::media::StalledPacketCounter first, second;
    for (int packet = 0; packet < 1000; ++packet) EXPECT_FALSE(first.observe(false));
    EXPECT_FALSE(second.observe(false));
    EXPECT_TRUE(first.observe(false));
    EXPECT_FALSE(second.observe(false));
}
