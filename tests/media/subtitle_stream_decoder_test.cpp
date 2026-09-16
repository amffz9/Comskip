#include "media/subtitle_stream_decoder.h"
#include <gtest/gtest.h>
#include <chrono>
#include <limits>
#include <string>

namespace {
using namespace comskip::media;
using namespace std::chrono_literals;
AVCodecParameters parameters(AVCodecID codec = AV_CODEC_ID_SUBRIP) {
    AVCodecParameters result{};
    result.codec_type = AVMEDIA_TYPE_SUBTITLE;
    result.codec_id = codec;
    return result;
}
std::span<const std::uint8_t> bytes(const std::string& value) {
    return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}
}
TEST(SubtitleStreamDecoder, SrtPacketPreservesNonzeroStartDurationUnicodeAndMarkup) {
    auto source = parameters();
    SubtitleStreamDecoder decoder(source, {1, 90000});
    const std::string text = "<i>Café 字幕</i>\nsecond line";
    const auto cues = decoder.decode(bytes(text), 450000, 180000);
    ASSERT_EQ(cues.size(), 1);
    EXPECT_EQ(cues[0].start, 5s);
    EXPECT_EQ(cues[0].end, 7s);
    ASSERT_EQ(cues[0].regions.size(), 1);
    EXPECT_NE(cues[0].regions[0].ass.find("Café 字幕"), std::string::npos);
    EXPECT_NE(cues[0].regions[0].ass.find("{\\i1}"), std::string::npos);
    EXPECT_NE(cues[0].regions[0].ass.find("\\Nsecond line"), std::string::npos);
    EXPECT_FALSE(decoder.ass_header().empty());
    const auto precise = decoder.decode(bytes("PRECISE"), 90001, 90001);
    ASSERT_EQ(precise.size(), 1);
    EXPECT_EQ(precise[0].start, 1000011us);
    EXPECT_EQ(precise[0].end, 2000022us);
}
TEST(SubtitleStreamDecoder, AssHeaderAndStyledEventRemainOwnedAfterCallerChangesParameters) {
    auto source = parameters(AV_CODEC_ID_ASS);
    std::string header = "[Script Info]\nScriptType: v4.00+\n[Events]\n";
    source.extradata = reinterpret_cast<std::uint8_t*>(header.data());
    source.extradata_size = static_cast<int>(header.size());
    SubtitleStreamDecoder decoder(source, {1, 1000});
    const auto saved_header = header;
    header.assign(header.size(), 'X');
    source.extradata = nullptr;
    source.extradata_size = 0;
    const std::string event = "7,0,Default,,0,0,0,,{\\b1}Styled é{\\b0}";
    auto cues = decoder.decode(bytes(event), 1250, 2250);
    ASSERT_EQ(cues.size(), 1);
    EXPECT_EQ(cues[0].start, 1250ms);
    EXPECT_EQ(cues[0].end, 3500ms);
    EXPECT_EQ(cues[0].regions[0].ass, event);
    EXPECT_EQ(decoder.ass_header(), saved_header);
    decoder.reset();
    EXPECT_EQ(decoder.ass_header(), saved_header);
    EXPECT_EQ(decoder.decode(bytes(event), 0, 1000)[0].regions[0].ass, event);
}
TEST(SubtitleStreamDecoder, InterleavedDecodersAndOutOfOrderEventsRemainIndependent) {
    auto source = parameters();
    SubtitleStreamDecoder first(source, {1, 1000}), second(source, {1, 90000});
    const auto alpha = first.decode(bytes("ALPHA"), 5000, 2000);
    const auto beta = second.decode(bytes("BETA"), 90000, 270000);
    const auto earlier = first.decode(bytes("EARLIER"), 2000, 4000);
    EXPECT_EQ(alpha[0].start, 5s);
    EXPECT_EQ(beta[0].start, 1s);
    EXPECT_EQ(earlier[0].end, 6s);
    EXPECT_NE(alpha[0].regions[0].ass.find("ALPHA"), std::string::npos);
    EXPECT_NE(beta[0].regions[0].ass.find("BETA"), std::string::npos);
}
TEST(SubtitleStreamDecoder, EofIsIdempotentAndResetStartsFreshTimeline) {
    auto source = parameters();
    SubtitleStreamDecoder decoder(source, {1, 1000});
    EXPECT_EQ(decoder.decode(bytes("LAST"), 8000, 1000).size(), 1);
    EXPECT_TRUE(decoder.drain().empty());
    EXPECT_TRUE(decoder.drain().empty());
    EXPECT_THROW(decoder.decode(bytes("AFTER"), 9000, 1000), std::logic_error);
    decoder.reset();
    auto cues = decoder.decode(bytes("FRESH"), 0, 500);
    ASSERT_EQ(cues.size(), 1);
    EXPECT_EQ(cues[0].start, 0us);
    EXPECT_EQ(cues[0].end, 500ms);
}
TEST(SubtitleStreamDecoder, InvalidTimesAndMalformedUtf8DoNotPoisonNextPacket) {
    auto source = parameters();
    SubtitleStreamDecoder decoder(source, {1, 1000});
    EXPECT_THROW(decoder.decode({}, 0, 1000), std::invalid_argument);
    EXPECT_THROW(decoder.decode(bytes("TEXT"), -1, 1000), std::invalid_argument);
    EXPECT_THROW(decoder.decode(bytes("TEXT"), 0, -1), std::invalid_argument);
    EXPECT_THROW(decoder.decode(bytes("TEXT"), std::numeric_limits<std::int64_t>::max(), 1000), std::invalid_argument);
    EXPECT_THROW(decoder.decode(bytes(std::string("BAD\xff")), 0, 1000), std::runtime_error);
    const auto cues = decoder.decode(bytes("RECOVERED"), 3000, 1000);
    ASSERT_EQ(cues.size(), 1);
    EXPECT_NE(cues[0].regions[0].ass.find("RECOVERED"), std::string::npos);
}
TEST(SubtitleStreamDecoder, RejectsBitmapStreamsAndInvalidParametersWithActionableError) {
    auto source = parameters(AV_CODEC_ID_DVD_SUBTITLE);
    try {
        SubtitleStreamDecoder decoder(source, {1, 1000});
        FAIL() << "Bitmap subtitles must require an explicit OCR path";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(std::string(error.what()).find("OCR"), std::string::npos);
    }
    source = parameters();
    EXPECT_THROW((SubtitleStreamDecoder{source, {0, 1000}}), std::invalid_argument);
    source.extradata_size = 2;
    EXPECT_THROW((SubtitleStreamDecoder{source, {1, 1000}}), std::invalid_argument);
    source = parameters(AV_CODEC_ID_NONE);
    EXPECT_THROW((SubtitleStreamDecoder{source, {1, 1000}}), std::invalid_argument);
}
