#include "output/frame_script_adapter.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include "diagnostic_render.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>

namespace {
class FrameScriptAdapter : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory=std::filesystem::temp_directory_path()/
            ("comskip-frame-script-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+
             "-"+std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context=std::make_unique<RecordingContext>();
        context->state.outbasename=comskip::platform::path_to_utf8(directory/"result");
        context->state.mpegfilename=context->state.outbasename+".ts";
        context->state.frame_count=50; context->state.framenum_real=50;
        context->settings.fps=25;
        context->settings.output_vcf=true; context->settings.output_projectx=true; context->settings.output_avisynth=true;
    }
    void TearDown() override { context.reset(); std::error_code ignored; std::filesystem::remove_all(directory,ignored); }
    std::string read(const char* suffix) {
        std::ifstream input(directory/(std::string("result")+suffix));
        return {std::istreambuf_iterator<char>(input),{}};
    }
};
TEST_F(FrameScriptAdapter, ActualEarlyCommercialsHaveJoinedTrimsAndLegacyFrameMapping) {
    context->state.reffer={{6,9},{20,30}}; context->state.reffer_count=1;
    WriteFrameScriptFiles(*context,true);
    EXPECT_EQ(read(".ts.Xcl"),"CollectionPanel.CutMode=2\n1\n7\n11\n21\n32\n49\n");
    EXPECT_EQ(read(".vcf"),"VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n"
        "VirtualDub.subset.AddRange(9,11);\nVirtualDub.subset.AddRange(30,18);\n");
    const auto script=read(".ts.avs");
    EXPECT_NE(script.find("trim(1,7) ++ trim(11,21) ++ trim(32,49)\n"),std::string::npos);
}
TEST_F(FrameScriptAdapter, NoCommercialsPreserveLegacyLeadingVcfOmissionAndSourceTemplateEscapes) {
    context->state.commercial_count=-1;
    context->settings.avisynth_options="%% source %s\n";
    WriteFrameScriptFiles(*context);
    EXPECT_EQ(read(".vcf"),"VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n");
    EXPECT_EQ(read(".ts.avs"),"% source "+context->state.mpegfilename+"\ntrim(1,49)\n");
}
TEST_F(FrameScriptAdapter, DecoderRealCountClampIsPreservedForIrregularFrameTimes) {
    context->state.commercial_count=-1;
    context->state.frame.resize(51); context->state.framenum_real=30;
    for (int i=0;i<51;++i) context->state.frame[i].pts=i*0.08;
    WriteFrameScriptFiles(*context);
    EXPECT_EQ(read(".ts.Xcl"),"CollectionPanel.CutMode=2\n3\n59\n");
}
TEST_F(FrameScriptAdapter, IndependentOutputSwitchesDoNotChangeAvisynthRanges) {
    context->state.reffer={{6,9},{20,30}}; context->state.reffer_count=1;
    WriteFrameScriptFiles(*context,true);
    const auto expected=read(".ts.avs");
    context->settings.output_vcf=false; context->settings.output_projectx=false;
    WriteFrameScriptFiles(*context,true);
    EXPECT_EQ(read(".ts.avs"),expected);
}
TEST_F(FrameScriptAdapter, InvalidGeometryAndRangesAreRejectedWithoutCreatingFiles) {
    context->state.reffer={{20,10}}; context->state.reffer_count=0;
    EXPECT_THROW(WriteFrameScriptFiles(*context,true),std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(directory/"result.vcf"));
    context->state.reffer.clear(); context->state.reffer_count=-1; context->settings.fps=0;
    EXPECT_THROW(WriteFrameScriptFiles(*context,true),std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(directory/"result.vcf"));
}
TEST_F(FrameScriptAdapter, UnwritableActualExportReportsSpanishAndPreservesBlockingDirectory) {
    context->state.commercial_count=-1;
    context->settings.output_projectx=false; context->settings.output_avisynth=false;
    context->translator=comskip::localization::Translator("es");
    ASSERT_TRUE(std::filesystem::create_directory(directory/"result.vcf"));
    try { WriteFrameScriptFiles(*context); FAIL()<<"Expected output creation rejection"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::output_open);
        EXPECT_EQ(comskip::localization::render_diagnostic(error.diagnostic(),context->translator),
                  "No se pudo abrir el archivo de salida: "+context->state.outbasename+".vcf");
    }
    EXPECT_TRUE(std::filesystem::is_directory(directory/"result.vcf"));
}
}
