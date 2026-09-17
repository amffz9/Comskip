#include "recording_context.h"
#include "media_dump.h"
#include "output/diagnostics.h"
#include "diagnostic_render.h"
#include "checked_format.h"
#include "output/csv_field.h"
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
TEST_F(DiagnosticOutput, AspectOutputPreservesLayoutAndClosesDestination) {
    context->settings.output_aspect = true;
    context->settings.fps = 25.0;
    context->state.ar_block.resize(1);
    context->state.ar_block_count = 1;
    context->state.ar_block[0] = {25, 50, 1.777, 0, 1080, 1920, 10, 1910, 20, 1060};

    OutputAspect(*context);

    EXPECT_EQ(read("log.aspects"), "0:00:01.00 1920x1080 1.78 minX=  10, minY=  20, maxX=1910, maxY=1060\n");
    // A closed CRT stream can be removed on Windows as well as POSIX.
    EXPECT_TRUE(std::filesystem::remove(directory / "log.aspects"));
}
TEST_F(DiagnosticOutput, FrameOutputPreservesLegacyDelimitedLayout) {
    std::array<unsigned char, 4> frame{0, 29, 30, 255};
    context->state.frame_ptr = frame.data();
    context->state.width = 2;
    context->state.videowidth = 2;
    context->state.height = 2;

    OutputFrame(*context, 7);

    EXPECT_EQ(read("log7.frm"), "0;;  0;  1\n  0;   ;   \n  1; 30;255\n");
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
TEST_F(DiagnosticOutput, ThresholdHistogramsRejectEmptyAndNegativeBinsBeforeOutputOrIndexing) {
    context->settings.output_training = true;
    for (const auto threshold : {FindBlackThreshold, FindUniformThreshold}) {
        try { threshold(*context, 0.95); FAIL() << "Expected invalid histogram"; }
        catch (const comskip::diagnostics::DiagnosticProvider& error) {
            EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::invalid_histogram_report);
        }
    }
    context->state.brightHistogram[0] = -1;
    context->state.uniformHistogram[0] = -1;
    EXPECT_THROW(FindBlackThreshold(*context,0.95),std::invalid_argument);
    EXPECT_THROW(FindUniformThreshold(*context,0.95),std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(directory / "black.csv"));
    EXPECT_FALSE(std::filesystem::exists(directory / "uniform.csv"));
}
TEST_F(DiagnosticOutput, ThresholdHistogramsAccumulateLargeBinCountsWithoutOverflow) {
    constexpr int large_bin = 1'500'000'000;
    context->state.brightHistogram[0] = large_bin;
    context->state.brightHistogram[1] = large_bin;
    context->state.uniformHistogram[0] = large_bin;
    context->state.uniformHistogram[1] = large_bin;
    EXPECT_EQ(FindBlackThreshold(*context,0.5),0);
    // A first-bin uniform threshold retains the legacy minimum-bin adjustment.
    EXPECT_EQ(FindUniformThreshold(*context,0.5),2 * 100);
}
TEST_F(DiagnosticOutput, TrainingThresholdReportsQuoteNamesAndCloseBothOutputs) {
    struct CurrentPathGuard {
        std::filesystem::path original = std::filesystem::current_path();
        ~CurrentPathGuard() { std::filesystem::current_path(original); }
    } guard;
    std::filesystem::current_path(directory);
    context->settings.output_training = true;
    context->state.inbasename = "recording, \"part\"";
    context->state.brightHistogram[0] = 10;
    context->state.uniformHistogram[0] = 10;

    EXPECT_EQ(FindBlackThreshold(*context, 0.5), 0);
    EXPECT_EQ(FindUniformThreshold(*context, 0.5), 2 * 100);

    std::string expected = comskip::output::csv_field(context->state.inbasename) + ",1000.00";
    for (int index = 1; index < 35; ++index) expected += ",  0.00";
    expected += '\n';
    EXPECT_EQ(read("black.csv"), expected);
    EXPECT_EQ(read("uniform.csv"), expected);
}
TEST_F(DiagnosticOutput, UnavailableOptionalTrainingReportsDoNotPreventThresholdSelection) {
    struct CurrentPathGuard {
        std::filesystem::path original = std::filesystem::current_path();
        ~CurrentPathGuard() { std::filesystem::current_path(original); }
    } guard;
    std::filesystem::current_path(directory);
    context->settings.output_training = true;
    context->state.brightHistogram[0] = 10;
    context->state.uniformHistogram[0] = 10;
    ASSERT_TRUE(std::filesystem::create_directory("black.csv"));
    ASSERT_TRUE(std::filesystem::create_directory("uniform.csv"));

    EXPECT_EQ(FindBlackThreshold(*context, 0.5), 0);
    EXPECT_EQ(FindUniformThreshold(*context, 0.5), 2 * 100);
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
