#include "a53_caption_bridge.h"
#include <gtest/gtest.h>
#include <array>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <string>

namespace {
using namespace comskip::media;
TEST(A53CaptionBridge, Preserves608XdsAnd708TripletsAndTerminatesPacket) {
    const std::array<std::uint8_t, 9> payload{0xfc, 0x94, 0x2c, 0xfd, 0x01, 0x02, 0xfe, 0x10, 0x20};
    const auto packets = bridge_a53_captions(payload);
    ASSERT_EQ(packets.size(), 1u); const auto& packet = packets.front();
    EXPECT_EQ(packet.size, 17u);
    EXPECT_EQ(std::string(packet.bytes.begin(), packet.bytes.begin() + 4), "GA94");
    EXPECT_EQ(packet.bytes[4], 3); EXPECT_EQ(packet.bytes[5], 0x43); EXPECT_EQ(packet.bytes[6], 0);
    EXPECT_TRUE(std::equal(payload.begin(), payload.end(), packet.bytes.begin() + 7));
    EXPECT_EQ(packet.bytes[packet.size - 1], 0xff);
}
TEST(A53CaptionBridge, ChunksPayloadBeyondLegacy500ByteBufferWithoutLoss) {
    std::vector<std::uint8_t> payload(3000);
    std::iota(payload.begin(), payload.end(), 0);
    const auto packets = bridge_a53_captions(payload);
    ASSERT_EQ(packets.size(), 33u);
    std::vector<std::uint8_t> recovered;
    for (const auto& packet : packets) {
        const auto count = packet.bytes[5] & 0x1f;
        ASSERT_GT(count, 0); ASSERT_LE(count, 31);
        EXPECT_EQ(packet.bytes[5] & 0xe0, 0x40);
        EXPECT_EQ(packet.size, 8u + count * 3u);
        EXPECT_LE(packet.size, ga94_max_packet_size);
        EXPECT_EQ(packet.bytes[packet.size - 1], 0xff);
        recovered.insert(recovered.end(), packet.bytes.begin() + 7, packet.bytes.begin() + 7 + count * 3);
    }
    EXPECT_EQ(recovered, payload);
}
TEST(A53CaptionBridge, CountBoundaryDoesNotWrapOrLoseThirtySecondTriplet) {
    for (const std::size_t count : {31u, 32u, 62u, 63u}) {
        const std::vector<std::uint8_t> payload(count * 3, 0xfc);
        const auto packets = bridge_a53_captions(payload);
        EXPECT_EQ(packets.size(), (count + 30) / 31);
        std::size_t recovered = 0;
        for (const auto& packet : packets) recovered += packet.bytes[5] & 0x1f;
        EXPECT_EQ(recovered, count);
    }
}
TEST(A53CaptionBridge, RejectsMalformedLengthsAndHandlesEmptyInput) {
    EXPECT_TRUE(bridge_a53_captions({}).empty());
    for (const std::size_t size : {1u, 2u, 4u, 499u, 500u, 1000u}) {
        const std::vector<std::uint8_t> payload(size);
        EXPECT_THROW(bridge_a53_captions(payload), std::invalid_argument);
    }
}
TEST(A53CaptionBridge, StoredGa94RoundTripsChunkedTripletsAndLegacyMissingTerminator) {
    const std::vector<std::uint8_t> payload(3000, 0xfc);
    std::vector<std::uint8_t> recovered;
    for (const auto& packet : bridge_a53_captions(payload)) {
        const auto decoded = extract_a53_captions({packet.bytes.data(), packet.size - 1});
        recovered.insert(recovered.end(), decoded.begin(), decoded.end());
    }
    EXPECT_EQ(recovered, payload);
    const std::array<std::uint8_t, 10> disabled{'G','A','9','4',3,1,0,0xfc,0x94,0x2c};
    EXPECT_TRUE(extract_a53_captions(disabled).empty());
}
TEST(A53CaptionBridge, StoredDvdPairsPreserveBothFieldOrders) {
    std::array<std::uint8_t, 12> packet{'C','C',1,0xf8,0x82,0xff,0x94,0x20,0xfe,0x91,0x22,0xff};
    EXPECT_EQ(extract_a53_captions(packet), (std::vector<std::uint8_t>{0xfc,0x94,0x20,0xfd,0x91,0x22}));
    packet[4] = 2; packet[5] = 0xfe; packet[8] = 0xff;
    EXPECT_EQ(extract_a53_captions(packet), (std::vector<std::uint8_t>{0xfd,0x94,0x20,0xfc,0x91,0x22}));
}
TEST(A53CaptionBridge, StoredRecognizedPacketsRejectTruncation) {
    const std::vector<std::vector<std::uint8_t>> packets{
        {'G','A','9','4'}, {'G','A','9','4',3,0x41,0,0xfc},
        {'C','C',1,0xf8,2,0xff,0x94}, {0xbb,2,0,0,0,0,0x94}};
    for (const auto& packet : packets) EXPECT_THROW(extract_a53_captions(packet), std::invalid_argument);
    EXPECT_TRUE(extract_a53_captions(std::array<std::uint8_t,4>{0,1,2,3}).empty());
}
}
