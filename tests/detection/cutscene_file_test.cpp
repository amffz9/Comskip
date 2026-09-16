#include "cutscene_file.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace comskip::detection;

TEST(CutsceneFile, EncodesSignedBrightnessAsPortableLittleEndian) {
    const CutsceneRecord record{-2, {0x11, 0x80, 0xff}};
    const auto bytes = encode_cutscene(record);
    ASSERT_TRUE(bytes);
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{0xfe, 0xff, 0xff, 0xff, 0x11, 0x80, 0xff}));
    EXPECT_EQ(decode_cutscene(*bytes), record);
}

TEST(CutsceneFile, DecodesExistingX86FourByteIntegerFiles) {
    const std::vector<std::uint8_t> bytes{0x78, 0x56, 0x34, 0x12, 1, 2};
    const auto record = decode_cutscene(bytes);
    ASSERT_TRUE(record);
    EXPECT_EQ(record->brightness, 0x12345678);
    EXPECT_EQ(record->pixels, (std::vector<std::uint8_t>{1, 2}));
}

TEST(CutsceneFile, RejectsEveryTruncatedRecordAndOversizedPayload) {
    for (std::size_t size = 0; size < 4; ++size) {
        std::vector<std::uint8_t> bytes(size);
        EXPECT_EQ(decode_cutscene(bytes).error(), CutsceneFileError::missing_header);
    }
    EXPECT_EQ(decode_cutscene(std::vector<std::uint8_t>(4)).error(), CutsceneFileError::missing_pixels);
    EXPECT_EQ(decode_cutscene(std::vector<std::uint8_t>(5 + maximum_cutscene_pixels)).error(),
              CutsceneFileError::too_many_pixels);
}

TEST(CutsceneFile, StreamRoundTripPreservesMaximumPayload) {
    CutsceneRecord source{255, std::vector<std::uint8_t>(maximum_cutscene_pixels, 42)};
    std::ostringstream output(std::ios::binary);
    ASSERT_TRUE(write_cutscene(output, source));
    std::istringstream input(output.str(), std::ios::binary);
    EXPECT_EQ(read_cutscene(input), source);
}

TEST(CutsceneFile, ReportsStreamWriteFailure) {
    std::ostringstream output;
    output.setstate(std::ios::badbit);
    EXPECT_EQ(write_cutscene(output, CutsceneRecord{1, {2}}).error(), CutsceneFileError::write_failed);
}
