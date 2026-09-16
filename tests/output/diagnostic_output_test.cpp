#include "recording_context.h"
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
#include <random>

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
    std::array<char, 4> payload{'A', '\0', static_cast<char>(0xff), '\n'};
    context->state.framenum_real = 42;
    dump_data(*context, payload.data(), static_cast<int>(payload.size()));
    close_data(*context);
    EXPECT_EQ(read("dump.data"), std::string("     42:   4") + std::string(payload.data(), payload.size()));
    EXPECT_FALSE(context->state.dump_data_file);
}
TEST_F(DiagnosticOutput, DataOpenFailureReportsSpanishAndReturnsSafely) {
    context->translator = comskip::localization::Translator("es");
    comskip::checked_format(context->state.workbasename, "%s", (directory / "missing" / "dump").string().c_str());
    char payload = 'x';
    EXPECT_NO_THROW(dump_data(*context, &payload, 1));
    EXPECT_FALSE(context->state.dump_data_file);
    const auto message = read("log.txt");
    EXPECT_NE(message.find("no se pudo crear el archivo"), std::string::npos);
    EXPECT_NE(message.find("dump.data"), std::string::npos);
}
TEST_F(DiagnosticOutput, InvalidBuffersAndFrameFieldsRejectBeforeOpeningOutput) {
    char payload = 'x';
    EXPECT_THROW(dump_data(*context, &payload, -1), std::invalid_argument);
    EXPECT_THROW(dump_data(*context, nullptr, 1), std::invalid_argument);
    context->state.framenum_real = 10000000;
    EXPECT_THROW(dump_data(*context, &payload, 1), std::out_of_range);
    EXPECT_FALSE(std::filesystem::exists(directory / "dump.data"));
    EXPECT_FALSE(context->state.dump_data_file);
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
}
TEST_F(DiagnosticOutput, ClosingDumpsAfterDisablingDemuxFlushesAndReleasesFiles) {
    context->settings.output_demux = true;
    dump_audio_start(*context);
    dump_video_start(*context);
    ASSERT_TRUE(context->state.dump_audio_file);
    ASSERT_TRUE(context->state.dump_video_file);
    const char audio[] = "buffered audio";
    const char video[] = "buffered video";
    ASSERT_EQ(std::fwrite(audio, 1, sizeof(audio) - 1, context->state.dump_audio_file.get()), sizeof(audio) - 1);
    ASSERT_EQ(std::fwrite(video, 1, sizeof(video) - 1, context->state.dump_video_file.get()), sizeof(video) - 1);
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
