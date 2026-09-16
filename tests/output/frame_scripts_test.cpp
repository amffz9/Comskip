#include "output/frame_scripts.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <locale>
#include <sstream>
using namespace comskip::output;

TEST(FrameScripts, ExactGoldenRangesPreserveVirtualDubAndProjectXSyntax) {
    const std::array vcf{VcfRange{9,11},VcfRange{30,18}};
    const std::array retained{ScriptFrameRange{1,7},ScriptFrameRange{11,21}};
    std::ostringstream video, project;
    write_vcf(video,vcf); write_projectx(project,retained);
    EXPECT_EQ(video.str(),"VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n"
        "VirtualDub.subset.AddRange(9,11);\nVirtualDub.subset.AddRange(30,18);\n");
    EXPECT_EQ(project.str(),"CollectionPanel.CutMode=2\n1\n7\n11\n21\n");
}
TEST(FrameScripts, EveryAdditionalAvisynthTrimHasAJoinOperator) {
    const std::array trims{ScriptFrameRange{1,7},ScriptFrameRange{11,21},ScriptFrameRange{32,49}};
    std::ostringstream output;
    write_avisynth(output,"source\n",trims);
    EXPECT_EQ(output.str(),"source\ntrim(1,7) ++ trim(11,21) ++ trim(32,49)\n");
}
TEST(FrameScripts, EmptyExportsPreserveHeadersAndTerminalNewline) {
    std::ostringstream video, project, avisynth;
    write_vcf(video,{}); write_projectx(project,{}); write_avisynth(avisynth,"source\n",{});
    EXPECT_EQ(video.str(),"VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n");
    EXPECT_EQ(project.str(),"CollectionPanel.CutMode=2\n");
    EXPECT_EQ(avisynth.str(),"source\n\n");
}
TEST(FrameScripts, InvalidLateRangesLeaveDestinationUntouched) {
    std::ostringstream output;
    const std::array invalid{ScriptFrameRange{0,1},ScriptFrameRange{2,1}};
    EXPECT_THROW(write_projectx(output,invalid),std::invalid_argument);
    EXPECT_THROW(write_avisynth(output,"source",invalid),std::invalid_argument);
    EXPECT_TRUE(output.str().empty());
    const std::array overflow{VcfRange{std::numeric_limits<std::int64_t>::max(),1}};
    EXPECT_THROW(write_vcf(output,overflow),std::invalid_argument);
    EXPECT_TRUE(output.str().empty());
}
TEST(FrameScripts, SingleFrameAndZeroLengthLegacyRecordsRemainRepresentable) {
    std::ostringstream output;
    const std::array trims{ScriptFrameRange{0,0}};
    write_avisynth(output,"",trims);
    EXPECT_EQ(output.str(),"trim(0,0)\n");
}
TEST(FrameScripts, FailedStreamIsReportedForAllFormats) {
    std::ostringstream output; output.setstate(std::ios::badbit);
    EXPECT_THROW(write_vcf(output,{}),std::runtime_error);
    EXPECT_THROW(write_projectx(output,{}),std::runtime_error);
    EXPECT_THROW(write_avisynth(output,"",{}),std::runtime_error);
}
TEST(FrameScripts, StreamLocaleDoesNotAddThousandsSeparators) {
    struct Grouping : std::numpunct<char> {
        std::string do_grouping() const override { return "\3"; }
        char do_thousands_sep() const override { return ','; }
    };
    std::ostringstream output; output.imbue(std::locale(std::locale::classic(),new Grouping));
    const std::array ranges{ScriptFrameRange{1000,2000}};
    write_projectx(output,ranges);
    EXPECT_EQ(output.str(),"CollectionPanel.CutMode=2\n1000\n2000\n");
}
