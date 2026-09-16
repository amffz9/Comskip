#include "output/ffmpeg_sidecar_adapter.h"
#include "output/frame_script_adapter.h"
#include "output/player_export_adapter.h"
#include "cutlist_exports.h"
#include "output/csv_field.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>

namespace {
class SidecarAdapter : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory=std::filesystem::temp_directory_path()/
            ("comskip-sidecar-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+
             "-"+std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context=std::make_unique<RecordingContext>();
        context->state.outbasename=comskip::platform::path_to_utf8(directory/"result");
        context->state.frame_count=50; context->state.framenum_real=50;
        context->settings.fps=25; context->state.output_console=false;
        context->settings.output_default=false;
        context->settings.output_ffmeta=true; context->settings.output_ffsplit=true;
    }
    void TearDown() override { context.reset(); std::error_code ignored; std::filesystem::remove_all(directory,ignored); }
    std::string read(const char* extension) {
        std::ifstream input(directory/(std::string("result")+extension));
        return {std::istreambuf_iterator<char>(input),{}};
    }
    std::string temporary_contents(FILE* file) {
        std::fflush(file); std::rewind(file);
        std::string result; int c;
        while ((c=std::fgetc(file))!=EOF) result.push_back(static_cast<char>(c));
        return result;
    }
};
TEST_F(SidecarAdapter, NoCommercialsPreserveLegacyTerminalGapAndCompleteHeader) {
    context->state.commercial_count=-1;
    WriteFfmpegSidecarFiles(*context);
    EXPECT_EQ(read(".ffmeta"),";FFMETADATA1\n[CHAPTER]\nTIMEBASE=1/100\nSTART=0\nEND=192\ntitle=Show Segment\n");
    EXPECT_EQ(read(".ffsplit"),"-c copy -ss 0.000 -t 1.920 segment000.ts \n");
}
TEST_F(SidecarAdapter, ReviewReferenceSelectionPreservesSegmentIndicesAndShortCuts) {
    context->state.reffer={{4,10},{20,30}}; context->state.reffer_count=1;
    WriteFfmpegSidecarFiles(*context,true);
    EXPECT_EQ(read(".ffsplit"),"-c copy -ss 0.440 -t 0.360 segment001.ts \n-c copy -ss 1.240 -t 0.680 segment002.ts \n");
    EXPECT_NE(read(".ffmeta").find("START=0\nEND=40\ntitle=Commercial Segment"),std::string::npos);
}
TEST_F(SidecarAdapter, EarlyEdlAdjustmentsDoNotChangeLaterBsPlayerExports) {
    context->settings.output_bsplayer=true;
    context->state.commercial.resize(1);
    context->state.commercial[0].start_frame=4;
    context->state.commercial[0].end_frame=10;
    context->state.commercial_count=0;
    OutputCommercialBlock(*context,0,-1,4,10,false);
    WritePlayerExportFiles(*context);
    const auto player=read(".bcf");
    for (auto* owner : {&context->state.edl_file,&context->state.edlp_file}) {
        *owner=comskip::platform::temporary_file(); ASSERT_TRUE(*owner);
    }
    OutputCommercialBlock(*context,0,-1,4,10,false);
    WritePlayerExportFiles(*context);
    EXPECT_EQ(read(".bcf"),player);
    EXPECT_EQ(player,"1,160,400\n");
}
TEST_F(SidecarAdapter, EnablingFfmetadataDoesNotChangeSimultaneousProjectXExport) {
    context->state.mpegfilename=context->state.outbasename;
    context->settings.output_default=false;
    context->settings.output_projectx=true;
    context->state.commercial.resize(1);
    context->state.commercial[0].start_frame=4;
    context->state.commercial[0].end_frame=10;
    context->state.commercial_count=0;
    std::string expected;
    for (const bool metadata : {false,true}) {
        context->settings.output_ffmeta=metadata;
        OpenOutputFiles(*context);
        OutputCommercialBlock(*context,0,-1,4,10,true);
        WriteFfmpegSidecarFiles(*context);
        WriteFrameScriptFiles(*context);
        const auto actual=read(".Xcl");
        if (!metadata) expected=actual;
        else EXPECT_EQ(actual,expected);
    }
    EXPECT_FALSE(expected.empty());
}
TEST_F(SidecarAdapter, Mpeg2SchnittDoesNotEnableTheUnrequestedMpgtxFormat) {
    context->settings.output_default=false; context->settings.output_ffmeta=false; context->settings.output_ffsplit=false;
    context->settings.output_mpeg2schnitt=true; context->settings.output_mpgtx=false;
    context->state.mpegfilename="input.ts";
    OpenOutputFiles(*context);
    EXPECT_FALSE(context->settings.output_mpgtx);
    EXPECT_TRUE(context->state.mpeg2schnitt_file);
}
TEST_F(SidecarAdapter, ActualStrictTrainingExportQuotesFilenameWithCommasQuotesAndNewlines) {
    context->state.inbasename="Café, \"recording\"\npart";
    context->state.training_file=comskip::platform::temporary_file();
    ASSERT_TRUE(context->state.training_file);
    OutputStrict(*context,1,2,3);
    EXPECT_EQ(temporary_contents(context->state.training_file.get()),
        "+1.000000,+2.000000,+3.000000,"+comskip::output::csv_field(context->state.inbasename)+"\n");
}
}
