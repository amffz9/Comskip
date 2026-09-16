#include "recording_context.h"
#include "diagnostic_render.h"
#include "detection/legacy_detection.h"
#include "checked_format.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <limits>
#include <random>
#include <vector>

namespace {
class DiagnosticOutput : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-dump-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 1;
        context->settings.output_data = true;
        context->state.output_console = false;
        comskip::checked_format(context->state.logfilename, "%s", (directory / "log.txt").string().c_str());
        comskip::checked_format(context->state.workbasename, "%s", (directory / "dump").string().c_str());
    }
    void TearDown() override {
        context.reset();
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    std::string read(const char* name) {
        std::ifstream input(directory / name, std::string{name} == "dump.data" ? std::ios::binary : std::ios::in);
        return {std::istreambuf_iterator<char>(input), {}};
    }
};
TEST_F(DiagnosticOutput, BinaryDataRetainsTwelveByteHeaderAndPayload) {
    std::array<std::uint8_t,4> payload{'A',0,0xff,'\n'};
    context->state.framenum_real = 42;
    dump_data(*context,payload);
    close_data(*context);
    EXPECT_EQ(read("dump.data"),std::string("     42:   4")+
        std::string(reinterpret_cast<const char*>(payload.data()),payload.size()));
    EXPECT_FALSE(context->state.dump_data_file);
}
TEST_F(DiagnosticOutput, DataOpenFailureReportsOwnedPathInSpanish) {
    context->translator = comskip::localization::Translator("es");
    comskip::checked_format(context->state.workbasename, "%s", (directory / "missing" / "dump").string().c_str());
    const std::array<std::uint8_t,1> payload{'x'};
    try { dump_data(*context,payload); FAIL() << "Expected output-open diagnostic"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::output_open);
        EXPECT_EQ(comskip::localization::render_diagnostic(error.diagnostic(),context->translator),
            "No se pudo abrir el archivo de salida: "+(directory/"missing"/"dump.data").string());
    }
    EXPECT_FALSE(context->state.dump_data_file);
}
TEST_F(DiagnosticOutput, EmptyOversizedAndInvalidFrameDataRejectBeforeOpeningOutput) {
    EXPECT_NO_THROW(dump_data(*context,{}));
    const std::vector<std::uint8_t> oversized(1901,'x');
    EXPECT_NO_THROW(dump_data(*context,oversized));
    const std::array<std::uint8_t,1> payload{'x'};
    context->state.framenum_real = 10000000;
    EXPECT_THROW(dump_data(*context,payload),std::out_of_range);
    EXPECT_FALSE(std::filesystem::exists(directory / "dump.data"));
    EXPECT_FALSE(context->state.dump_data_file);
}
TEST_F(DiagnosticOutput, ActualCsvBufferBoundsPreserveRangeCategoryAndRenderEnglishSpanish) {
    context->state.frame_count=2;
    context->state.frame.resize(2);
    try {OutputFrameArray(*context,false); FAIL()<<"Expected frame storage rejection";}
    catch(const std::out_of_range& error) {
        EXPECT_EQ(comskip::localization::render_exception(error,comskip::localization::Translator("en")),
            "CSV observations exceed the frame buffer");
        EXPECT_EQ(comskip::localization::render_exception(error,comskip::localization::Translator("es")),
            "Las observaciones CSV exceden el búfer de fotogramas");
    }
    EXPECT_FALSE(std::filesystem::exists(directory/"log.csv"));
}
TEST_F(DiagnosticOutput, InvalidCsvObservationRejectsBeforeCreatingDestination) {
    context->state.frame_count=1;
    context->state.frame.resize(2);
    context->state.frame[1].pts=std::numeric_limits<double>::quiet_NaN();
    try { OutputFrameArray(*context,false); FAIL() << "Expected invalid CSV output"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::invalid_frame_csv_output);
    }
    EXPECT_FALSE(std::filesystem::exists(directory/"log.csv"));
}
TEST_F(DiagnosticOutput, AspectOutputOpenFailureIsLocalized) {
    context->translator = comskip::localization::Translator("es");
    context->settings.output_aspect = true;
    // A directory at the destination forces fopen failure while keeping the log writable.
    ASSERT_TRUE(std::filesystem::create_directory(directory / "log.aspects"));
    OutputAspect(*context);
    EXPECT_EQ(read("log.txt"), "No se pudo abrir el archivo de salida de relaciones de aspecto.\n");
}
TEST_F(DiagnosticOutput, ScreenFrameOutputUsesInitializedLogoValueAndFinalObservation) {
    context->state.frame_count = 1;
    context->state.frame.resize(2);
    context->state.frame[1].brightness = 23;
    context->state.frame[1].schange_percent = 45;
    context->state.frame[1].logo_present = 1;
    ::testing::internal::CaptureStdout();
    OutputFrameArray(*context, true);
    auto output = ::testing::internal::GetCapturedStdout();
    std::erase(output, '\r');
    EXPECT_EQ(output, "1\t23\t45\t1\tHistogram\n");
    EXPECT_FALSE(std::filesystem::exists(directory/"log.csv"));
}
TEST_F(DiagnosticOutput, EmptyHistogramsAvoidNonfiniteOutputAndRejectNegativeCounts) {
    context->state.framesprocessed=0;
    EXPECT_NO_THROW(OutputbrightHistogram(*context));
    const auto message=read("log.txt");
    EXPECT_EQ(message.find("nan"),std::string::npos);
    EXPECT_EQ(message.find("inf"),std::string::npos);
    context->state.brightHistogram[4]=-1;
    try { OutputbrightHistogram(*context); FAIL() << "Expected invalid histogram"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::invalid_histogram_report);
    }
}
TEST_F(DiagnosticOutput, ClosingDumpsAfterDisablingDemuxFlushesAndReleasesFiles) {
    context->settings.output_demux = true;
    dump_audio_start(*context);
    dump_video_start(*context);
    ASSERT_TRUE(context->state.dump_audio_file);
    ASSERT_TRUE(context->state.dump_video_file);
    const std::array<std::uint8_t,14> audio{'b','u','f','f','e','r','e','d',' ','a','u','d','i','o'};
    const std::array<std::uint8_t,14> video{'b','u','f','f','e','r','e','d',' ','v','i','d','e','o'};
    dump_audio(*context,audio);
    dump_video(*context,video);
    context->settings.output_demux = false;
    close_dump(*context);
    EXPECT_EQ(read("dump.mp2"), "buffered audio");
    EXPECT_EQ(read("dump.m2v"), "buffered video");
    EXPECT_FALSE(context->state.dump_audio_file);
    EXPECT_FALSE(context->state.dump_video_file);
    // Windows rejects removal of an open CRT file; POSIX still checks flushing.
    EXPECT_TRUE(std::filesystem::remove(directory / "dump.mp2"));
    EXPECT_TRUE(std::filesystem::remove(directory / "dump.m2v"));
    EXPECT_NO_THROW(close_dump(*context));
}
}
