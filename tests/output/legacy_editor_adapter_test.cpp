#include "output/legacy_editor_adapter.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
namespace{
class LegacyEditorAdapter:public ::testing::Test{
protected:
    std::filesystem::path directory;std::unique_ptr<RecordingContext> context;
    void SetUp()override{
        directory=std::filesystem::temp_directory_path()/("comskip-legacy-editor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));context=std::make_unique<RecordingContext>();
        context->state.outbasename=comskip::platform::path_to_utf8(directory/"result");
        context->state.mpegfilename=comskip::platform::path_to_utf8(directory/"input.ts");
        context->state.frame_count=100;context->state.framenum_real=100;context->settings.fps=25;
        context->settings.output_vdr=true;context->settings.output_videoredo=true;context->settings.output_videoredo3=false;
    }
    void TearDown()override{context.reset();std::error_code ignored;std::filesystem::remove_all(directory,ignored);}
    std::string read(const char* suffix){std::ifstream in(directory/(std::string("result")+suffix));return {std::istreambuf_iterator<char>(in),{}};}
};
TEST_F(LegacyEditorAdapter, ActualReviewCutOffsetsAndNormalSceneTimesRetainDifferentClamps){
    context->state.reffer={{4,40}};context->state.reffer_count=0;context->settings.videoredo_offset=2;
    context->state.demux_pid=1;context->state.selected_video_pid=100;context->state.selected_audio_pid=200;context->state.selected_subtitle_pid=300;
    context->state.block_count=1;context->state.cblock.resize(2,comskip::detection::empty_block());context->state.cblock[0].f_end=80;
    WriteLegacyEditorFiles(*context,true);
    EXPECT_EQ(read(".vdr"),"0:00:00.00 start\n0:00:01.15 end\n");
    const auto project=read(".VPrj");
    EXPECT_NE(project.find("<Cut>400000:14800000\n"),std::string::npos);
    EXPECT_NE(project.find("<SceneMarker 0>30800000\n"),std::string::npos);
    EXPECT_NE(project.find("<VideoStreamPID>100\n"),std::string::npos);
}
TEST_F(LegacyEditorAdapter, EmptyNormalListPreservesCompleteProjectAndEmptyVdr){
    context->state.commercial_count=-1;WriteLegacyEditorFiles(*context);
    EXPECT_TRUE(read(".vdr").empty());
    EXPECT_EQ(read(".VPrj"),"<Version>2\n<Filename>"+context->state.mpegfilename+"\n");
}
TEST_F(LegacyEditorAdapter, OutputSwitchAndModernVideoRedoModeDoNotChangeVdrMarks){
    context->state.reffer={{4,40}};context->state.reffer_count=0;WriteLegacyEditorFiles(*context,true);
    const auto expected=read(".vdr");context->settings.output_videoredo3=true;
    WriteLegacyEditorFiles(*context,true);EXPECT_EQ(read(".vdr"),expected);
}
TEST_F(LegacyEditorAdapter, InvalidCommercialIntervalsAreRejectedBeforeCreatingFiles){
    context->state.reffer={{40,4}};context->state.reffer_count=0;
    EXPECT_THROW(WriteLegacyEditorFiles(*context,true),std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(directory/"result.VPrj"));
}
TEST_F(LegacyEditorAdapter, NegativeOffsetsPreserveUnclampedFallbackTiming){
    context->state.reffer={{4,40}};context->state.reffer_count=0;
    context->settings.videoredo_offset=-100;
    WriteLegacyEditorFiles(*context,true);
    EXPECT_NE(read(".VPrj").find("<Cut>41200000:55600000\n"),std::string::npos);
}
}
