#include "media/caption_session.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

namespace {
using namespace comskip::media;
using namespace std::chrono_literals;
class StandaloneCaptionSession : public ::testing::Test {
protected:
    std::filesystem::path directory;
    AVCodecParameters parameters{};
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip standalone " + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        parameters.codec_type = AVMEDIA_TYPE_SUBTITLE;
        parameters.codec_id = AV_CODEC_ID_SUBRIP;
    }
    void TearDown() override { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    static std::span<const std::uint8_t> bytes(const std::string& text) {
        return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
    }
    std::string read(const char* file) {
        std::ifstream input(directory / file, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }
};
}
TEST_F(StandaloneCaptionSession, InterleavedOutputsOwnOriginClippingAndNumbering) {
    CaptionSession first({directory / "first", true, true}), second({directory / "second", true, false});
    first.select_stream(parameters, {1, 1000}); second.select_stream(parameters, {1, 1000});
    first.consume_stream(bytes("ALPHA"), 4500, 1500, 5s);
    second.consume_stream(bytes("BETA"), 3000, 2000, 1s);
    first.consume_stream(bytes("NEXT"), 7000, 1000, 5s);
    first.finish(4s); second.finish(5s);
    EXPECT_NE(read("first.srt").find("00:00:00,000 --> 00:00:01,000"), std::string::npos);
    EXPECT_NE(read("first.srt").find("2\n00:00:02,000 --> 00:00:03,000"), std::string::npos);
    EXPECT_NE(read("second.srt").find("00:00:02,000 --> 00:00:04,000"), std::string::npos);
    EXPECT_EQ(read("first.srt").find("BETA"), std::string::npos);
    EXPECT_NE(read("first.smi").find("ALPHA"), std::string::npos);
}
TEST_F(StandaloneCaptionSession, ReopenPreservesCompletedCuesAndResetReplacesPreviousPass) {
    CaptionSession session({directory / "recording", true, false});
    session.select_stream(parameters, {1, 1000});
    session.consume_stream(bytes("BEFORE REOPEN"), 1000, 1000, 0s);
    session.select_stream(parameters, {1, 1000});
    session.consume_stream(bytes("AFTER REOPEN"), 3000, 1000, 0s);
    session.finish(4s);
    EXPECT_NE(read("recording.srt").find("BEFORE REOPEN"), std::string::npos);
    EXPECT_NE(read("recording.srt").find("AFTER REOPEN"), std::string::npos);
    session.reset();
    session.consume_stream(bytes("FRESH"), 0, 1000, 0s);
    session.finish(1s);
    EXPECT_EQ(read("recording.srt").find("REOPEN"), std::string::npos);
    EXPECT_NE(read("recording.srt").find("1\n00:00:00,000"), std::string::npos);
}
TEST_F(StandaloneCaptionSession, BitmapSelectionAndMalformedPacketDoNotPoisonValidSession) {
    CaptionSession session({directory / "recording", true, false});
    auto bitmap = parameters; bitmap.codec_id = AV_CODEC_ID_DVD_SUBTITLE;
    EXPECT_THROW(session.select_stream(bitmap, {1, 1000}), std::invalid_argument);
    session.select_stream(parameters, {1, 1000});
    EXPECT_THROW(session.consume_stream(bytes(std::string("BAD\xff")), 0, 1000, 0s), std::runtime_error);
    session.consume_stream(bytes("RECOVERED"), 2000, 1000, 0s);
    session.finish(3s);
    EXPECT_NE(read("recording.srt").find("RECOVERED"), std::string::npos);
}
