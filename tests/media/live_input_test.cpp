#include "media/live_input.h"

#include <gtest/gtest.h>

namespace {

TEST(LiveInput, FollowsPlainAndDriveQualifiedFiles)
{
    EXPECT_TRUE(comskip::media::follows_growing_file("/media/captures/commercials.ts"));
    EXPECT_TRUE(comskip::media::follows_growing_file("recording.ts"));
    EXPECT_TRUE(comskip::media::follows_growing_file(R"(F:\Recordings\show.ts)"));
    EXPECT_TRUE(comskip::media::follows_growing_file("file:/media/show.ts"));
}

TEST(LiveInput, LeavesProtocolsAndEmptyNamesUnchanged)
{
    EXPECT_FALSE(comskip::media::follows_growing_file("pipe:0"));
    EXPECT_FALSE(comskip::media::follows_growing_file("http://tuner/auto/v5.1"));
    EXPECT_FALSE(comskip::media::follows_growing_file(""));
}

TEST(LiveInput, TimeoutKeepsTheLegacyRetryBudget)
{
    EXPECT_EQ(comskip::media::growing_file_timeout_us(12), 48'000'000);
    EXPECT_EQ(comskip::media::growing_file_timeout_us(0), 4'000'000);
    EXPECT_EQ(comskip::media::growing_file_timeout_us(-3), 4'000'000);
}

}
