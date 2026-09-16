#include "output/frame_csv.h"
#include "input/frame_record.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <locale>
#include <sstream>
#include <streambuf>

namespace {
TEST(FrameCsv, WritesReplayableModernRowsAndFinalObservationExactly) {
    std::array<frame_info,2> frames{};
    frames[0].brightness=10; frames[0].schange_percent=3; frames[0].logo_present=true;
    frames[0].uniform=4; frames[0].volume=5; frames[0].minY=6; frames[0].maxY=7;
    frames[0].ar_ratio=1.25; frames[0].currentGoodEdge=0.5; frames[0].isblack=8;
    frames[0].cutscenematch=9; frames[0].minX=10; frames[0].maxX=11;
    frames[0].hasBright=12; frames[0].dimCount=13; frames[0].pts=1.5;
    frames[0].cur_segment=14; frames[0].audio_channels=6;
    frames[1]=frames[0]; frames[1].brightness=99; frames[1].pts=2.5;
    std::ostringstream output;
    comskip::output::write_frame_csv(output,frames,{25});
    std::istringstream lines(output.str());
    std::string separator,header,first,last;
    ASSERT_TRUE(static_cast<bool>(std::getline(lines,separator)));
    ASSERT_TRUE(static_cast<bool>(std::getline(lines,header)));
    ASSERT_TRUE(static_cast<bool>(std::getline(lines,first)));
    ASSERT_TRUE(static_cast<bool>(std::getline(lines,last)));
    EXPECT_EQ(separator,"sep=,");
    EXPECT_EQ(header,"frame,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,isblack,cutscene, MinX, MaxX, hasBright, Dimcount,PTS,25.000000");
    const auto parsed=comskip::input::parse_frame_record(first);
    EXPECT_EQ(parsed.number,1); EXPECT_EQ(parsed.brightness,10); EXPECT_EQ(parsed.scene_change,3);
    EXPECT_EQ(parsed.logo,1); EXPECT_DOUBLE_EQ(parsed.timestamp.value(),1.5);
    EXPECT_EQ(parsed.segment,14); EXPECT_EQ(parsed.audio_channels,6);
    EXPECT_EQ(comskip::input::parse_frame_record(last).brightness,99);
}

TEST(FrameCsv, EmptyOutputHasOnlyReplayableHeaderAndRejectsInvalidRates) {
    std::ostringstream output;
    comskip::output::write_frame_csv(output,{}, {30000.0/1001.0});
    EXPECT_EQ(output.str(),"sep=,\nframe,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,isblack,cutscene, MinX, MaxX, hasBright, Dimcount,PTS,29.970030\n");
    EXPECT_THROW(comskip::output::write_frame_csv(output,{}, {0}),std::invalid_argument);
    EXPECT_THROW(comskip::output::write_frame_csv(output,{}, {std::numeric_limits<double>::infinity()}),std::invalid_argument);
    frame_info invalid{};
    invalid.pts=std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(comskip::output::write_frame_csv(output,std::span{&invalid,1},{25}),std::invalid_argument);
    invalid.pts=0; invalid.schange_percent=std::numeric_limits<int>::max();
    EXPECT_THROW(comskip::output::write_frame_csv(output,std::span{&invalid,1},{25}),std::invalid_argument);
}

class FailingBuffer final : public std::streambuf {
    std::streamsize xsputn(const char*,std::streamsize) override { return 0; }
    int overflow(int value) override { return traits_type::eq_int_type(value,traits_type::eof()) ? value : traits_type::eof(); }
};
TEST(FrameCsv, ReportsDestinationFailure) {
    FailingBuffer buffer;
    std::ostream output(&buffer);
    EXPECT_THROW(comskip::output::write_frame_csv(output,{}, {25}),std::ios_base::failure);
}
}
