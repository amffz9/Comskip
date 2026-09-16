#include "input/reference_file.h"
#include "input/frame_record.h"
#include "input/checked_number.h"
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
using namespace comskip::input;
ReferenceFile reference(std::string value, std::size_t limit = 100000) {
    std::istringstream source(value); return read_reference_file(source, limit);
}
constexpr std::string_view header = "FILE PROCESSING COMPLETE    150 FRAMES AT  2500\n-------------------\n";
TEST(TextInput, ReadsFinalLineWithoutNewlineAndRejectsOversizedOrNullLines) {
    std::istringstream source("final"); EXPECT_EQ(read_text_line(source), "final"); EXPECT_FALSE(read_text_line(source));
    std::istringstream oversized(std::string(maximum_text_line + 1, 'x'));
    EXPECT_THROW(read_text_line(oversized), std::length_error);
    std::istringstream nul(std::string("a\0b", 3)); EXPECT_THROW(read_text_line(nul), std::invalid_argument);
}
TEST(ReferenceInput, PreservesWhitespaceIntervalsReversalAndMissingFinalNewline) {
    const auto parsed = reference(std::string(header) + "  20\t40\r\n80 60");
    EXPECT_EQ(parsed.declared_frames, 150); EXPECT_EQ(parsed.frames_per_second, 25);
    ASSERT_EQ(parsed.intervals.size(), 2); EXPECT_EQ(parsed.intervals[0].start_frame, 20);
    EXPECT_EQ(parsed.intervals[0].end_frame, 40); EXPECT_EQ(parsed.intervals[1].end_frame, 60);
}
TEST(ReferenceInput, RejectsEmptyPartialHeadersAndInvalidNumbers) {
    for (const auto value : {"", "FILE PROCESSING", "FILE PROCESSING COMPLETE 150 FRAMES AT 2500\n"})
        EXPECT_THROW(reference(value), std::invalid_argument);
    for (const auto row : {"1 nope", "1", "1 2 garbage", "-1 2", "2147483648 3", "1 2.0"})
        EXPECT_THROW(reference(std::string(header) + row), std::invalid_argument);
}
TEST(ReferenceInput, BoundsLongTokensAndIntervalCountWithoutPartialReturn) {
    EXPECT_THROW(reference(std::string(header) + std::string(2047, '9') + " 2"), std::invalid_argument);
    EXPECT_THROW(reference(std::string(header) + "1 2\n3 4\n", 1), std::length_error);
    EXPECT_EQ(reference(std::string(header) + "1 2\n", 1).intervals.size(), 1);
}
TEST(FrameRecord, LibraryDecodesCommaSemicolonQuotesAndOptionalLegacyColumns) {
    for (const auto row : {"1,20,25,1,2,30,0,120,1.333333,0.5,0", "1;20;25;1;2;30;0;120;133;250;0;",
                           "\"1\",20,25,1,2,30,0,120,1.333333,0.5,0\r\n"}) {
        const auto parsed = parse_frame_record(row);
        EXPECT_EQ(parsed.number, 1); EXPECT_EQ(parsed.scene_change, 5); EXPECT_EQ(parsed.audio_channels, 2);
        EXPECT_NEAR(parsed.aspect_ratio, 1.333333, 0.004); EXPECT_EQ(parsed.good_edge, 0.5);
        EXPECT_FALSE(parsed.timestamp);
    }
}
TEST(FrameRecord, PreservesAllModernColumnsWithoutTrailingNewline) {
    const auto parsed = parse_frame_record("2,20,25,1,2,30,0,120,1.33,0.5,4,3,1,159,7,8,0.04,9,6");
    EXPECT_EQ(parsed.number, 2); EXPECT_EQ(parsed.black, 4); EXPECT_EQ(parsed.cutscene_match, 3);
    EXPECT_EQ(parsed.min_x, 1); EXPECT_EQ(parsed.max_x, 159); EXPECT_EQ(parsed.bright_count, 7);
    EXPECT_EQ(parsed.dim_count, 8); EXPECT_EQ(parsed.timestamp, 0.04); EXPECT_EQ(parsed.segment, 9);
    EXPECT_EQ(parsed.audio_channels, 6);
}
TEST(FrameRecord, RejectsMalformedNonfiniteAndOutOfRangeValues) {
    for (const auto row : {"", "1,2", "2147483648,20,25,1,2,30,0,120,1.33,0.5,0",
                           "1,bad,25,1,2,30,0,120,1.33,0.5,0", "1,20,25,1,2,30,0,120,nan,0.5,0",
                           "1,20,25,1,2,30,0,120,1.33,inf,0", "1,20,25,1,2,30,0,120,1.33,0.5,0,0,0,0,0,0,-1"})
        EXPECT_THROW(parse_frame_record(row), std::invalid_argument);
    EXPECT_THROW(parse_frame_record(std::string(maximum_text_line + 1, '9')), std::length_error);
    EXPECT_THROW(parse_frame_record("1," + std::string(2047, '9') + ",25,1,2,30,0,120,1.33,0.5,0"), std::invalid_argument);
}
TEST(CheckedNumber, RetainsWhitespacePlusAndExponentWithoutLocaleDependence) {
    EXPECT_EQ(parse_number<int>(" +42 \r", "field"), 42);
    EXPECT_EQ(parse_number<double>("1.5e2", "field"), 150);
    EXPECT_THROW(parse_number<int>("", "field"), std::invalid_argument);
    EXPECT_THROW(parse_number<int>("+-1", "field"), std::invalid_argument);
    EXPECT_THROW(parse_number<int>("2 trailing", "field"), std::invalid_argument);
    EXPECT_THROW(parse_number<double>("1,5", "field"), std::invalid_argument);
}
}
