#include "recording_context.h"
#include "detection/legacy_detection.h"
#include "checked_format.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string_view>

namespace {
class Diagnostics : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-diagnostic-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 2;
        context->state.output_console = false;
        comskip::checked_format(context->state.logfilename, "%s", (directory / "log.txt").string().c_str());
    }
    void TearDown() override {
        context.reset();
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    std::string log() {
        std::ifstream input(directory / "log.txt", std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }
};
TEST_F(Diagnostics, WritesWholeLongUnicodeMessageWithLegacyFormatArguments) {
    // Exceeds both the original 2,000-byte buffer and its later 20,000-byte size.
    const auto unicode = std::u8string_view{u8"Advertencia: grabación 日本語 "};
    const std::string fragment(reinterpret_cast<const char*>(unicode.data()), unicode.size());
    std::string message;
    for (int i = 0; i < 1000; ++i) message += fragment;
    ASSERT_GT(message.size(), 20000u);
    Debug(*context, 2, "%s[%d/%04x] %.3f", message.c_str(), 42, 255, 1.25);
    EXPECT_EQ(log(), message + "[42/00ff] 1.250");
    Debug(*context, 1, "|%s:%d|", "next", 7);
    EXPECT_EQ(log(), message + "[42/00ff] 1.250|next:7|");
}
TEST_F(Diagnostics, VerboseGatingDoesNotCreateOrAppendLog) {
    Debug(*context, 3, "%s", "hidden");
    EXPECT_FALSE(std::filesystem::exists(directory / "log.txt"));
    Debug(*context, 2, "%s", "shown");
    Debug(*context, 3, "%d", 999);
    EXPECT_EQ(log(), "shown");
}
}
