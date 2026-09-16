#include "caption_decoder.h"
#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace {
using namespace comskip::media;
using namespace std::chrono_literals;
using Pair = std::pair<std::uint8_t, std::uint8_t>;
std::uint8_t parity(std::uint8_t value) {
    return value | ((std::popcount(static_cast<unsigned>(value)) % 2 == 0) ? 0x80 : 0);
}
std::vector<std::uint8_t> triplets(std::initializer_list<Pair> pairs, CaptionField field = CaptionField::first) {
    std::vector<std::uint8_t> result;
    for (const auto [hi, lo] : pairs) {
        result.push_back(field == CaptionField::first ? 0xfc : 0xfd);
        result.push_back(parity(hi)); result.push_back(parity(lo));
    }
    return result;
}
std::string ass(const CaptionCue& cue) {
    std::string result;
    for (const auto& region : cue.regions) result += region.ass;
    return result;
}
void popon(CaptionDecoder& decoder, Pair characters, CaptionTimestamp start,
           CaptionField field = CaptionField::first) {
    EXPECT_TRUE(decoder.decode(triplets({{0x14, 0x20}, {0x14, 0x2e}, {0x14, 0x60}, characters}, field), start - 1s).empty());
    EXPECT_TRUE(decoder.decode(triplets({{0x14, 0x2f}}, field), start).empty());
}
TEST(CaptionDecoder, PoponAppearsAtEndOfCaptionAndEraseClosesOwnedCue) {
    CaptionDecoder decoder;
    EXPECT_TRUE(decoder.ass_header().contains("[V4+ Styles]"));
    popon(decoder, {'H', 'i'}, 2s);
    auto cues = decoder.decode(triplets({{0x14, 0x2c}}), 5s);
    ASSERT_EQ(cues.size(), 1u); EXPECT_EQ(cues[0].start, 2s); EXPECT_EQ(cues[0].end, 5s);
    EXPECT_TRUE(ass(cues[0]).contains("Hi"));
    decoder.reset();
    EXPECT_TRUE(ass(cues[0]).contains("Hi")); // All text survives AVSubtitle/context release.
    EXPECT_TRUE(decoder.drain(10s).empty());
}
TEST(CaptionDecoder, EofClosesLastScreenAndResetRestoresFreshState) {
    CaptionDecoder decoder;
    popon(decoder, {'O', 'K'}, 2s);
    const auto cues = decoder.drain(7s);
    ASSERT_EQ(cues.size(), 1u); EXPECT_EQ(cues[0].start, 2s); EXPECT_EQ(cues[0].end, 7s);
    EXPECT_TRUE(ass(cues[0]).contains("OK"));
    EXPECT_TRUE(decoder.drain(7s).empty());
    EXPECT_THROW(decoder.decode(triplets({{'N', 'O'}}), 8s), std::logic_error);
    decoder.reset();
    popon(decoder, {'N', 'O'}, 2s);
    auto next = decoder.drain(4s);
    ASSERT_EQ(next.size(), 1u); EXPECT_TRUE(ass(next[0]).contains("NO")); EXPECT_FALSE(ass(next[0]).contains("OK"));
}
TEST(CaptionDecoder, RollupRetainsPreviousLineAndEmitsScreenChanges) {
    CaptionDecoder decoder;
    EXPECT_TRUE(decoder.decode(triplets({{0x14, 0x25}, {0x14, 0x60}, {'O', 'n'}, {'e', 0}}), 1s).empty());
    auto line = decoder.decode(triplets({{0x14, 0x2d}, {'T', 'w'}, {'o', 0}}), 2s);
    ASSERT_EQ(line.size(), 1u); EXPECT_TRUE(ass(line[0]).contains("One"));
    auto final = decoder.drain(4s);
    ASSERT_EQ(final.size(), 1u);
    EXPECT_TRUE(ass(final[0]).contains("One")); EXPECT_TRUE(ass(final[0]).contains("Two"));
    EXPECT_TRUE(ass(final[0]).contains("\\N"));
    EXPECT_EQ(final[0].start, 2s); EXPECT_EQ(final[0].end, 4s);
}
TEST(CaptionDecoder, ExtendedCharactersAndItalicStyleRemainInAss) {
    CaptionDecoder decoder;
    EXPECT_TRUE(decoder.decode(triplets({{0x14, 0x20}, {0x14, 0x2e}, {0x14, 0x60},
        {0x11, 0x2e}, {'X', 0}, {0x12, 0x20}}), 1s).empty());
    EXPECT_TRUE(decoder.decode(triplets({{0x14, 0x2f}}), 2s).empty());
    auto cues = decoder.drain(5s);
    ASSERT_EQ(cues.size(), 1u); EXPECT_TRUE(ass(cues[0]).contains("Á"));
    EXPECT_TRUE(ass(cues[0]).contains("\\i1"));
}
TEST(CaptionDecoder, MalformedLengthsAndBackwardTimesLeaveDisplayUsable) {
    CaptionDecoder decoder;
    popon(decoder, {'H', 'i'}, 2s);
    const std::array<std::uint8_t, 2> malformed{0xfc, 0x80};
    EXPECT_THROW(decoder.decode(malformed, 3s), std::invalid_argument);
    EXPECT_THROW(decoder.decode({}, -1s), std::invalid_argument);
    EXPECT_THROW(decoder.decode({}, 1s), std::invalid_argument);
    EXPECT_THROW(decoder.drain(1s), std::invalid_argument);
    auto cues = decoder.drain(4s);
    ASSERT_EQ(cues.size(), 1u); EXPECT_EQ(cues[0].start, 2s); EXPECT_TRUE(ass(cues[0]).contains("Hi"));
}
TEST(CaptionDecoder, TwoInterleavedDecodersOwnIndependentFieldsAndScreens) {
    CaptionDecoder first, second(CaptionField::second);
    popon(first, {'A', 'A'}, 2s);
    popon(second, {'B', 'B'}, 3s, CaptionField::second);
    EXPECT_TRUE(first.decode(triplets({{'X', 'X'}}, CaptionField::second), 4s).empty());
    EXPECT_TRUE(second.decode(triplets({{'Y', 'Y'}}), 4s).empty());
    first.reset();
    popon(first, {'C', 'C'}, 2s);
    auto b = second.drain(6s), c = first.drain(5s);
    ASSERT_EQ(b.size(), 1u); ASSERT_EQ(c.size(), 1u);
    EXPECT_TRUE(ass(b[0]).contains("BB")); EXPECT_FALSE(ass(b[0]).contains("CC"));
    EXPECT_TRUE(ass(c[0]).contains("CC")); EXPECT_FALSE(ass(c[0]).contains("AA"));
    EXPECT_EQ(b[0].start, 3s); EXPECT_EQ(c[0].start, 2s);
}
}
