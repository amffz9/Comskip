#include "recording_context.h"
#include "output/diagnostics.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>

namespace {
class ReferenceComparisonApplication : public ::testing::Test {
protected:
    std::filesystem::path directory, previous_directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-reference-comparison-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        previous_directory = std::filesystem::current_path();
        // Existing training output is relative to the process working directory.
        // Each Google Test executable has its own process and isolated directory.
        std::filesystem::current_path(directory);
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 0; context->settings.fps = 25;
        context->settings.output_training = 1;
        context->state.output_console = false;
        context->state.logfilename = "record.log";
        context->state.inbasename = "input";
    }
    void TearDown() override {
        context.reset();
        std::filesystem::current_path(previous_directory);
        std::error_code ignored; std::filesystem::remove_all(directory, ignored);
    }
    void reference(std::string_view rows, int frames = 400000) {
        std::ofstream file("record.ref");
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file << "FILE PROCESSING COMPLETE " << frames << " FRAMES AT 2500\n-------------------\n" << rows;
    }
    std::string read(std::string_view filename) {
        std::ifstream file{std::string(filename)};
        if (!file) throw std::runtime_error("Missing reference comparison output");
        return {std::istreambuf_iterator<char>(file), {}};
    }
};
TEST_F(ReferenceComparisonApplication, EmptyDetectionScoresValidReferenceWithoutAccessingNegativeIndex) {
    context->state.commercial_count = -1;
    reference("100 200\n");
    EXPECT_EQ(InputReffer(*context, ".ref", 0), 400000);
    EXPECT_EQ(read("quality.csv"), "\"input\",     -1,    4.0,    0.0,    4.0\n");
    EXPECT_TRUE(read("record.dif").contains("Reference    100    200"));
    EXPECT_TRUE(context->state.commercial.empty());
}
TEST_F(ReferenceComparisonApplication, BothEmptyListsProduceZeroMetricsAndNoSyntheticStoredIntervals) {
    context->state.commercial_count = -1;
    reference("");
    EXPECT_EQ(InputReffer(*context, ".ref", 0), 400000);
    EXPECT_EQ(read("quality.csv"), "\"input\",     -1,    0.0,    0.0,    0.0\n");
    EXPECT_TRUE(read("record.dif").empty());
    EXPECT_TRUE(context->state.reffer.empty());
}
TEST_F(ReferenceComparisonApplication, FullDetectedCapacityNeedsNoAdditionalSlotAndPreservesEveryInterval) {
    context->state.commercial.resize(100001);
    for (int index = 0; index < static_cast<int>(std::size(context->state.commercial)); ++index) {
        context->state.commercial[index].start_frame = index * 4;
        context->state.commercial[index].end_frame = index * 4 + 3;
    }
    context->state.commercial_count = static_cast<int>(std::size(context->state.commercial)) - 1;
    reference("", 400004);
    EXPECT_EQ(InputReffer(*context, ".ref", 0), 400004);
    EXPECT_EQ(read("quality.csv"), "\"input\",     -1,    0.0, 12000.1,    0.0\n");
    EXPECT_EQ(context->state.commercial_count, 100000);
    for (int index = 0; index <= context->state.commercial_count; ++index) {
        ASSERT_EQ(context->state.commercial[index].start_frame, index * 4);
        ASSERT_EQ(context->state.commercial[index].end_frame, index * 4 + 3);
    }
}
TEST_F(ReferenceComparisonApplication, FullReferenceCapacityIsAcceptedWithoutReservingAnArtificialSentinel) {
    context->settings.fps = 1; // Exact integer mapping across all 100,000 frame pairs.
    std::string rows;
    for (int index = 0; index < 100001; ++index)
        rows += std::to_string(index * 4) + " " + std::to_string(index * 4 + 3) + "\n";
    reference(rows, 400004);
    EXPECT_EQ(InputReffer(*context, ".ref", 0), 400004);
    EXPECT_EQ(context->state.reffer_count, 100000);
    EXPECT_EQ(read("quality.csv"), "\"input\",     -1, 300003.0,    0.0, 300003.0\n");
}
TEST_F(ReferenceComparisonApplication, PopulatedListsPreserveLegacyToleranceAndExactMixedMetrics) {
    context->state.commercial_count = 1;
    context->state.commercial.resize(2);
    context->state.commercial[0].start_frame = 150; context->state.commercial[0].end_frame = 250;
    context->state.commercial[1].start_frame = 300; context->state.commercial[1].end_frame = 460;
    reference("100 200\n300 400\n");
    EXPECT_EQ(InputReffer(*context, ".ref", 0), 400000);
    EXPECT_EQ(read("quality.csv"), "\"input\",     -1,    2.0,    4.4,    8.0\n");
    const auto difference = read("record.dif");
    EXPECT_TRUE(difference.contains("Found    150    250"));
    EXPECT_TRUE(difference.contains("Found    300    460"));
}
TEST_F(ReferenceComparisonApplication, InvalidCountsRejectBeforeAccessingStorage) {
    reference("");
    context->state.commercial_count = 100000;
    EXPECT_THROW(InputReffer(*context, ".ref", 0), std::out_of_range);
    context->state.commercial_count = -2;
    EXPECT_THROW(InputReffer(*context, ".ref", 0), std::out_of_range);
}
}
