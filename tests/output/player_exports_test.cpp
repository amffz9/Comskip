#include "output/player_exports.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <locale>
#include <sstream>
using namespace comskip::output;

TEST(PlayerExports, ZoomPlayerAndBsPlayerPreserveCommandBytes) {
    const std::array intervals{PlayerInterval{PlayerSeconds{1.239},PlayerSeconds{2.5}}};
    std::ostringstream cuts,player;
    write_zoomplayer_cuts(cuts,intervals); write_bsplayer(player,intervals);
    EXPECT_EQ(cuts.str(),"JumpSegment(\"From=1.2390\",\"To=2.5000\")\n");
    EXPECT_EQ(player.str(),"1,1239,2500\n");
}
TEST(PlayerExports, ZoomChapterNamesAndWholeSecondTruncationRemainStable) {
    const std::array marks{PlayerChapterMark{PlayerSeconds{1.239},3,PlayerBoundary::commercial_start},
        PlayerChapterMark{PlayerSeconds{2.5},4,PlayerBoundary::commercial_end}};
    std::ostringstream output;
    write_zoomplayer_chapters(output,marks,true);
    EXPECT_EQ(output.str(),"AddChapter(1,Show Segment)\nAddChapterBySecond(1,Commercial Segment)\nAddChapterBySecond(2,Show Segment)\n");
}
TEST(PlayerExports, ScfFractionUsesMillisecondsAndHoursDoNotWrapAtSixty) {
    const std::array marks{ScfFrameMark{37,1,PlayerBoundary::commercial_start},
        ScfFrameMark{61LL*3600*25+12,2,PlayerBoundary::commercial_end}};
    std::ostringstream output; write_scf(output,marks,{25});
    EXPECT_EQ(output.str(),"CHAPTER01=00:00:01.480\nCHAPTER01NAME=Commercial starts\n"
        "CHAPTER02=61:00:00.480\nCHAPTER02NAME=Commercial ends\n");
}
TEST(PlayerExports, IpOdLegacyHeaderCentisecondsAndNumberingGapsRemainStable) {
    const std::array marks{PlayerChapterMark{PlayerSeconds{1.239},4,PlayerBoundary::commercial_end}};
    std::ostringstream output; write_ipod_chapters(output,marks);
    EXPECT_EQ(output.str(),"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\nCHAPTER04=0:00:01.23\nCHAPTER04NAME=4\n");
}
TEST(PlayerExports, EmptyExportsAreCompleteWithoutInventedCommercialChapters) {
    std::ostringstream zoom,scf,ipod;
    write_zoomplayer_chapters(zoom,{},false); write_scf(scf,{},{25}); write_ipod_chapters(ipod,{});
    EXPECT_TRUE(zoom.str().empty()); EXPECT_TRUE(scf.str().empty());
    EXPECT_EQ(ipod.str(),"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n");
}
TEST(PlayerExports, InvalidLateRecordsDoNotPartiallyWriteDestination) {
    const std::array intervals{PlayerInterval{PlayerSeconds{0},PlayerSeconds{1}},
        PlayerInterval{PlayerSeconds{2},PlayerSeconds{1}}};
    std::ostringstream output;
    EXPECT_THROW(write_zoomplayer_cuts(output,intervals),std::invalid_argument);
    EXPECT_THROW(write_bsplayer(output,intervals),std::invalid_argument);
    EXPECT_THROW(write_scf(output,{},{0}),std::invalid_argument);
    const std::array marks{PlayerChapterMark{PlayerSeconds{INFINITY},2,PlayerBoundary::commercial_end}};
    EXPECT_THROW(write_ipod_chapters(output,marks),std::invalid_argument);
    EXPECT_TRUE(output.str().empty());
}
TEST(PlayerExports, FailedStreamsAreReportedForAllFiveFormats) {
    std::ostringstream output; output.setstate(std::ios::badbit);
    EXPECT_THROW(write_zoomplayer_chapters(output,{},false),std::runtime_error);
    EXPECT_THROW(write_zoomplayer_cuts(output,{}),std::runtime_error);
    EXPECT_THROW(write_scf(output,{},{25}),std::runtime_error);
    EXPECT_THROW(write_ipod_chapters(output,{}),std::runtime_error);
    EXPECT_THROW(write_bsplayer(output,{}),std::runtime_error);
}
TEST(PlayerExports, StreamLocaleDoesNotChangeDecimalSeparator) {
    struct Comma : std::numpunct<char> { char do_decimal_point() const override {return ',';} };
    std::ostringstream output; output.imbue(std::locale(std::locale::classic(),new Comma));
    const std::array intervals{PlayerInterval{PlayerSeconds{0.5},PlayerSeconds{1.25}}};
    write_zoomplayer_cuts(output,intervals);
    EXPECT_EQ(output.str(),"JumpSegment(\"From=0.5000\",\"To=1.2500\")\n");
}
