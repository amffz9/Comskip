#include "input/caption_packet.h"

#include <gtest/gtest.h>
#include <sstream>

namespace {
using comskip::input::CaptionPacketError;
using comskip::input::read_persisted_caption_packet;

TEST(CaptionPacket, ReadsRecordsAndCleanEndOfFile) {
    std::istringstream source("     12:   3abc      0:   0");
    auto first = read_persisted_caption_packet(source, 500);
    ASSERT_TRUE(first); ASSERT_TRUE(*first);
    EXPECT_EQ((*first)->frame, 12);
    EXPECT_EQ((*first)->payload, (std::vector<std::uint8_t>{'a', 'b', 'c'}));
    auto second = read_persisted_caption_packet(source, 500);
    ASSERT_TRUE(second); ASSERT_TRUE(*second);
    EXPECT_EQ((*second)->frame, 0); EXPECT_TRUE((*second)->payload.empty());
    auto end = read_persisted_caption_packet(source, 500);
    ASSERT_TRUE(end); EXPECT_FALSE(*end);
}

TEST(CaptionPacket, RejectsMalformedAndNegativeFields) {
    for (const auto text : {"      1;   0", "     -1:   0", " number:   0"}) {
        std::istringstream source(text);
        auto result = read_persisted_caption_packet(source, 500);
        ASSERT_FALSE(result); EXPECT_EQ(result.error(), CaptionPacketError::invalid_frame_header);
    }
    for (const auto text : {"      1: bad", "      1:  -1", "      1: 501"}) {
        std::istringstream source(text);
        auto result = read_persisted_caption_packet(source, 500);
        ASSERT_FALSE(result); EXPECT_EQ(result.error(), CaptionPacketError::invalid_packet_length);
    }
}

TEST(CaptionPacket, DistinguishesEveryTruncatedField) {
    struct Case { const char* text; CaptionPacketError error; };
    for (const auto& test : {Case{"   1", CaptionPacketError::truncated_frame_header},
                            Case{"      1: 2", CaptionPacketError::truncated_packet_length},
                            Case{"      1:   3ab", CaptionPacketError::truncated_packet}}) {
        std::istringstream source(test.text);
        auto result = read_persisted_caption_packet(source, 500);
        ASSERT_FALSE(result); EXPECT_EQ(result.error(), test.error);
    }
}

TEST(CaptionPacket, EnforcesCallerPayloadLimitBeforeAllocating) {
    std::istringstream source("      1: 500");
    auto result = read_persisted_caption_packet(source, 499);
    ASSERT_FALSE(result); EXPECT_EQ(result.error(), CaptionPacketError::invalid_packet_length);
}
}
