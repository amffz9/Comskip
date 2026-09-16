#include "recording_context.h"
#include "checked_format.h"
#include "exit_requested.h"
#include "output/diagnostics.h"
#include "input/reference_file.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>

namespace {
class ApplicationTextInput : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-text-input-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 0;
        context->state.output_console = false;
        context->settings.fps = 25;
        context->settings.commDetectMethod = 1;
        context->settings.num_logo_buffers = 2;
        context->settings.output_framearray = false;
        comskip::checked_format(context->state.logfilename, "%s", (directory / "record.log").string().c_str());
        comskip::checked_format(context->state.inbasename, "%s", (directory / "input").string().c_str());
        comskip::checked_format(context->state.workbasename, "%s", (directory / "output").string().c_str());
        comskip::checked_format(context->state.outbasename, "%s", (directory / "output").string().c_str());
        comskip::checked_format(context->state.out_filename, "%s", (directory / "output.txt").string().c_str());
    }
    void TearDown() override {
        context.reset();
        std::error_code ignored; std::filesystem::remove_all(directory, ignored);
    }
    void write(const std::filesystem::path& path, std::string_view content) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.exceptions(std::ios::failbit | std::ios::badbit); file << content;
    }
    void csv(std::string_view content) {
        const auto path = directory / "input.csv"; write(path, content);
        auto file = comskip::platform::own_file(myfopen(path.string().c_str(), "rb"));
        ASSERT_TRUE(file); ProcessCSV(*context, std::move(file));
    }
    static constexpr std::string_view reference_header = "FILE PROCESSING COMPLETE 150 FRAMES AT 2500\n-------------------\n";
    static constexpr std::string_view csv_header = "sep=,\nframe,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,isblack,cutscene,MinX,MaxX,hasBright,Dimcount,PTS,25.000000\n";
};
TEST_F(ApplicationTextInput, ReferenceEmptyAndPartialFilesRejectWithoutChangingOwnedState) {
    context->state.reffer_count = 7; context->state.reffer[7].start_frame = 123;
    for (const auto content : {"", "FILE PROCESSING COMPLETE", "FILE PROCESSING COMPLETE 150 FRAMES AT 2500\n"}) {
        write(directory / "record.txt", content);
        EXPECT_THROW(InputReffer(*context, ".txt", 1), std::invalid_argument);
        EXPECT_EQ(context->state.reffer_count, 7); EXPECT_EQ(context->state.reffer[7].start_frame, 123);
        EXPECT_EQ(context->settings.fps, 25);
    }
    EXPECT_TRUE(std::filesystem::remove(directory / "record.txt"));
}
TEST_F(ApplicationTextInput, ReferenceLongTokenMissingEndAndInvalidNumberRejectAtomically) {
    context->state.reffer_count = 7;
    for (const auto& row : {std::string(2047, '9') + " 2", std::string("1"), std::string("1 bad"),
                           std::string(comskip::input::maximum_text_line + 1, '9')}) {
        write(directory / "record.txt", std::string(reference_header) + "1 2\n" + row);
        EXPECT_THROW(InputReffer(*context, ".txt", 0), std::exception);
        EXPECT_EQ(context->state.reffer_count, 7);
    }
}
TEST_F(ApplicationTextInput, ReferenceCapacityReservesComparisonSentinelAndRejectsOverflow) {
    context->state.reffer_count = 7;
    std::string text(reference_header);
    for (std::size_t i = 0; i < std::size(context->state.reffer); ++i) text += "1 2\n";
    write(directory / "record.ref", text);
    EXPECT_THROW(InputReffer(*context, ".ref", 0), std::length_error);
    EXPECT_EQ(context->state.reffer_count, 7);
    text += "1 2\n"; write(directory / "record.txt", text);
    EXPECT_THROW(InputReffer(*context, ".txt", 0), std::length_error);
    EXPECT_EQ(context->state.reffer_count, 7);
}
TEST_F(ApplicationTextInput, ReferenceWhitespaceAndMissingFinalNewlinePreserveFrameConversion) {
    context->state.frame.resize(102); context->state.frame_count = 100;
    for (int frame = 0; frame <= 101; ++frame) context->state.frame[frame].pts = (frame - 1) / 25.0;
    const auto start = FindFrameWithPts(*context, 20 / 25.0), end = FindFrameWithPts(*context, 40 / 25.0);
    write(directory / "record.txt", std::string(reference_header) + " 20\t40");
    EXPECT_EQ(InputReffer(*context, ".txt", 0), 150);
    EXPECT_EQ(context->state.reffer_count, 0);
    EXPECT_EQ(context->state.reffer[0].start_frame, start); EXPECT_EQ(context->state.reffer[0].end_frame, end);
}
TEST_F(ApplicationTextInput, CsvMalformedFilesRejectBeforeAnyObservationOrSettingMutation) {
    context->state.frame_count = 77;
    for (const auto& content : {std::string{}, std::string("sep=,"), std::string(csv_header),
                               std::string(csv_header) + "1,2", std::string(csv_header) + "1," + std::string(2047, '9') + ",25,1,2,30,0,120,1.33,0.5,0",
                               std::string(csv_header) + "1,20,25,1,2,30,0,120,nan,0.5,0",
                               std::string(csv_header) + "2,20,25,1,2,30,0,120,1.33,0.5,0"}) {
        EXPECT_THROW(csv(content), std::invalid_argument);
        EXPECT_EQ(context->state.frame_count, 77); EXPECT_TRUE(context->state.frame.empty());
        EXPECT_EQ(context->settings.fps, 25);
        EXPECT_TRUE(std::filesystem::remove(directory / "input.csv"));
    }
}
TEST_F(ApplicationTextInput, CsvFinalObservationWithoutNewlineKeepsAllTypedColumnsAndDuration) {
    std::string text(csv_header);
    for (int frame = 1; frame <= 150; ++frame) {
        text += std::to_string(frame) + ",80,0,0,10,40,1,119,1.333333,0.5,0,0,1,159,7,8," +
            std::to_string((frame - 1) / 25.0) + ",9,6";
        if (frame != 150) text += '\n';
    }
    try { csv(text); FAIL() << "CSV application should return through ExitRequested"; }
    catch (const comskip::ExitRequested& requested) { EXPECT_EQ(requested.status(), 0); }
    ASSERT_EQ(context->state.frame_count, 150);
    EXPECT_DOUBLE_EQ(context->state.frame[150].pts, 5.96);
    EXPECT_EQ(context->state.frame[149].hasBright, 7); EXPECT_EQ(context->state.frame[149].dimCount, 8);
    // BuildMasterCommList deliberately clears these two terminal metrics when
    // inserting its final black boundary (detection.cpp). Parsing preserves the
    // preceding observation and all other stored terminal columns.
    EXPECT_EQ(context->state.frame[150].hasBright, 0); EXPECT_EQ(context->state.frame[150].dimCount, 0);
    EXPECT_EQ(context->state.frame[150].cur_segment, 9); EXPECT_EQ(context->state.frame[150].audio_channels, 6);
}
}
