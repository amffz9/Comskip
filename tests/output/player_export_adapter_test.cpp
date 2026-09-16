#include "output/player_export_adapter.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include "exit_requested.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>

namespace {
class PlayerAdapter : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory=std::filesystem::temp_directory_path()/
            ("comskip-player-export-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+
             "-"+std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context=std::make_unique<RecordingContext>();
        context->state.outbasename=comskip::platform::path_to_utf8(directory/"result");
        context->state.frame_count=150; context->state.framenum_real=150;
        context->settings.fps=25;
        context->settings.output_zoomplayer_cutlist=true; context->settings.output_zoomplayer_chapter=true;
        context->settings.output_scf=true; context->settings.output_ipodchap=true; context->settings.output_bsplayer=true;
    }
    void TearDown() override {context.reset(); std::error_code ignored; std::filesystem::remove_all(directory,ignored);}
    std::string read(const char* suffix) {
        std::ifstream input(directory/(std::string("result")+suffix));
        return {std::istreambuf_iterator<char>(input),{}};
    }
};
TEST_F(PlayerAdapter, EmptyNormalListProducesCompleteExportsWithoutAccessingCommercialZero) {
    context->state.commercial.clear(); context->state.commercial_count=-1;
    WritePlayerExportFiles(*context);
    EXPECT_TRUE(read(".chp").empty()); EXPECT_TRUE(read(".cut").empty()); EXPECT_TRUE(read(".scf").empty());
    EXPECT_TRUE(read(".bcf").empty());
    EXPECT_EQ(read(".chap"),"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n");
    for (const char* extension : {".chp",".cut",".scf",".bcf",".chap"})
        EXPECT_TRUE(std::filesystem::is_regular_file(directory/(std::string("result")+extension)));
}
TEST_F(PlayerAdapter, ReviewMarksUseSelectedListAndPreserveFormatSpecificFiltersAndNumberGaps) {
    context->state.reffer={{10,12},{37,80}}; context->state.reffer_count=1;
    WritePlayerExportFiles(*context,true);
    EXPECT_EQ(read(".chp"),"AddChapter(1,Show Segment)\nAddChapterBySecond(1,Commercial Segment)\nAddChapterBySecond(3,Show Segment)\n");
    EXPECT_EQ(read(".scf"),"CHAPTER03=00:00:01.480\nCHAPTER03NAME=Commercial starts\n"
        "CHAPTER04=00:00:03.200\nCHAPTER04NAME=Commercial ends\n");
    EXPECT_EQ(read(".cut"),"JumpSegment(\"From=1.4800\",\"To=3.2000\")\n");
    EXPECT_EQ(read(".bcf"),"1,1480,3200\n");
    EXPECT_NE(read(".chap").find("CHAPTER03NAME=3\n"),std::string::npos);
    EXPECT_EQ(read(".chap").find("CHAPTER02NAME"),std::string::npos);
}
TEST_F(PlayerAdapter, IrregularPtsAffectTimeExportsWhileScfRetainsRawFrameTiming) {
    context->state.commercial.resize(1); context->state.commercial_count=0;
    context->state.commercial[0].start_frame=37; context->state.commercial[0].end_frame=80;
    context->state.frame.resize(151);
    for(int i=0;i<151;++i) context->state.frame[i].pts=i*0.08;
    WritePlayerExportFiles(*context);
    EXPECT_EQ(read(".cut"),"JumpSegment(\"From=2.9600\",\"To=6.4000\")\n");
    EXPECT_NE(read(".scf").find("00:00:01.480"),std::string::npos);
}
TEST_F(PlayerAdapter, FormatSwitchesDoNotChangeOtherOutputs) {
    context->state.reffer={{37,80}}; context->state.reffer_count=0;
    WritePlayerExportFiles(*context,true);
    const auto expected=read(".bcf");
    context->settings.output_zoomplayer_cutlist=false; context->settings.output_zoomplayer_chapter=false;
    context->settings.output_scf=false; context->settings.output_ipodchap=false;
    WritePlayerExportFiles(*context,true);
    EXPECT_EQ(read(".bcf"),expected);
}
TEST_F(PlayerAdapter, ActualCreateFailureReportsSpanishAndPreservesBlockingDirectory) {
    context->state.commercial_count=-1;
    context->translator=comskip::localization::Translator("es");
    ASSERT_TRUE(std::filesystem::create_directory(directory/"result.chp"));
    ::testing::internal::CaptureStderr();
    try {WritePlayerExportFiles(*context); FAIL()<<"Expected export failure";}
    catch(const comskip::ExitRequested& exit) {EXPECT_EQ(exit.status(),6);}
    const auto message=::testing::internal::GetCapturedStderr();
    EXPECT_NE(message.find("no se pudo crear el archivo"),std::string::npos);
    EXPECT_TRUE(std::filesystem::is_directory(directory/"result.chp"));
}
}
