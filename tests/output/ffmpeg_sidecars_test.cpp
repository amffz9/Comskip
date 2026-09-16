#include "output/ffmpeg_sidecars.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <locale>
#include <sstream>
using namespace comskip::output;

TEST(FfmpegSidecars, NativeMuxerPreservesExistingMetadataBytesAndCentisecondTruncation) {
    const std::array chapters{SidecarChapter{SidecarSeconds{0.009},SidecarSeconds{1.239},SidecarSegmentKind::show},
        SidecarChapter{SidecarSeconds{1.239},SidecarSeconds{2.001},SidecarSegmentKind::commercial}};
    std::ostringstream output;
    write_ffmetadata(output, chapters);
    EXPECT_EQ(output.str(), ";FFMETADATA1\n[CHAPTER]\nTIMEBASE=1/100\nSTART=0\nEND=123\ntitle=Show Segment\n"
        "[CHAPTER]\nTIMEBASE=1/100\nSTART=123\nEND=200\ntitle=Commercial Segment\n");
}
TEST(FfmpegSidecars, EmptyMetadataStillHasHeaderAndEmptySplitHasNoCommands) {
    std::ostringstream metadata, split;
    write_ffmetadata(metadata, {}); write_ffsplit(split, {});
    EXPECT_EQ(metadata.str(), ";FFMETADATA1\n"); EXPECT_TRUE(split.str().empty());
}
TEST(FfmpegSidecars, SplitPreservesFractionalTimesNumberingAndTrailingSpace) {
    const std::array segments{SidecarShowSegment{SidecarSeconds{0},SidecarSeconds{1.2346},0},SidecarShowSegment{SidecarSeconds{2.5},SidecarSeconds{4},7}};
    std::ostringstream output;
    write_ffsplit(output, segments);
    EXPECT_EQ(output.str(), "-c copy -ss 0.000 -t 1.235 segment000.ts \n-c copy -ss 2.500 -t 1.500 segment007.ts \n");
}
TEST(FfmpegSidecars, InvalidLateIntervalsLeaveDestinationUntouched) {
    const std::array chapters{SidecarChapter{SidecarSeconds{0},SidecarSeconds{1},SidecarSegmentKind::show},
        SidecarChapter{SidecarSeconds{2},SidecarSeconds{1},SidecarSegmentKind::commercial}};
    std::ostringstream output;
    EXPECT_THROW(write_ffmetadata(output, chapters), std::invalid_argument);
    EXPECT_TRUE(output.str().empty());
    const std::array segments{SidecarShowSegment{SidecarSeconds{0},SidecarSeconds{1},0},SidecarShowSegment{SidecarSeconds{0},SidecarSeconds{1},-1}};
    EXPECT_THROW(write_ffsplit(output, segments), std::invalid_argument);
    EXPECT_TRUE(output.str().empty());
}
TEST(FfmpegSidecars, NonfiniteAndSignedTimestampBoundaryAreRejectedBeforeMuxing) {
    std::ostringstream output;
    const std::array nonfinite{SidecarChapter{SidecarSeconds{0},SidecarSeconds{INFINITY},SidecarSegmentKind::show}};
    EXPECT_THROW(write_ffmetadata(output, nonfinite), std::invalid_argument);
    const std::array overflow{SidecarChapter{SidecarSeconds{0},SidecarSeconds{std::ldexp(1.0,63)/100},SidecarSegmentKind::show}};
    EXPECT_THROW(write_ffmetadata(output, overflow), std::out_of_range);
    EXPECT_TRUE(output.str().empty());
}
TEST(FfmpegSidecars, OutputFailureIsActionableForBothFormats) {
    std::ostringstream output; output.setstate(std::ios::badbit);
    EXPECT_THROW(write_ffmetadata(output, {}), std::runtime_error);
    EXPECT_THROW(write_ffsplit(output, {}), std::runtime_error);
}
TEST(FfmpegSidecars, DecimalFormattingDoesNotUseTheStreamLocale) {
    struct Comma : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
    std::ostringstream output; output.imbue(std::locale(std::locale::classic(),new Comma));
    const std::array segments{SidecarShowSegment{SidecarSeconds{0.5},SidecarSeconds{1.75},0}};
    write_ffsplit(output, segments);
    EXPECT_EQ(output.str(), "-c copy -ss 0.500 -t 1.250 segment000.ts \n");
}
