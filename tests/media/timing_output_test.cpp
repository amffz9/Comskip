#include "recording_context.h"
#include "media/timing_diagnostics.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
constexpr auto header = "sep=,\ntype   ,real_pts, step        ,pts         ,clock       ,delta       ,offset, repeat\n";
const std::string audio_row = "a frame, " + std::string(7, ' ') + "1.250, " +
    std::string(7, ' ') + "0.040, " + std::string(7, ' ') + "1.500, " +
    std::string(7, ' ') + "1.250, " + std::string(7, ' ') + "0.250, " +
    std::string(6, ' ') + "-0.125, 2\n";
std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
class TimingOutput : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-timing-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->state.inbasename = utf8(directory / "録画");
        context->settings.output_timing = true;
    }
    void TearDown() override {
        context.reset(); std::error_code ignored; std::filesystem::remove_all(directory, ignored);
    }
    std::string contents() {
        std::ifstream file(directory / "録画.timing.csv");
        if (!file) throw std::runtime_error("Missing timing CSV");
        return {std::istreambuf_iterator<char>(file), {}};
    }
    void row() {
        comskip::media::write_timing_row(*context, "a frame", 1.25, 0.04, 1.5, 1.25, -0.125, 2);
    }
};
TEST_F(TimingOutput, ExactAudioAndVideoRowsCarryExplicitDelayAndDelta) {
    ASSERT_TRUE(comskip::media::open_timing_diagnostics(*context));
    row();
    comskip::media::write_timing_row(*context, "v clock", 2, -0.04, 2, 2.25, 0, 1);
    comskip::media::close_timing_diagnostics(*context);
    const std::string video = "v clock, " + std::string(7, ' ') + "2.000, " +
        std::string(6, ' ') + "-0.040, " + std::string(7, ' ') + "2.000, " +
        std::string(7, ' ') + "2.250, " + std::string(6, ' ') + "-0.250, " +
        std::string(7, ' ') + "0.000, 1\n";
    EXPECT_EQ(contents(), header + audio_row + video);
    EXPECT_FALSE(context->state.timing_file);
}
TEST_F(TimingOutput, AllSeekGuardsSuppressRowsThenWritingResumes) {
    ASSERT_TRUE(comskip::media::open_timing_diagnostics(*context));
    context->state.csStepping = 1; row(); context->state.csStepping = 0;
    context->state.csJumping = 1; row(); context->state.csJumping = 0;
    context->state.csStartJump = 1; row(); context->state.csStartJump = 0;
    row();
    comskip::media::close_timing_diagnostics(*context);
    EXPECT_EQ(contents(), header + audio_row);
}
TEST_F(TimingOutput, DisabledOpeningAndFailedCreationRemainOptionalAndRecover) {
    context->settings.output_timing = false;
    EXPECT_FALSE(comskip::media::open_timing_diagnostics(*context));
    row(); comskip::media::write_timing_header(*context);
    EXPECT_FALSE(std::filesystem::exists(directory / "録画.timing.csv"));
    context->settings.output_timing = true;
    context->state.inbasename = utf8(directory / "missing" / "録画");
    EXPECT_FALSE(comskip::media::open_timing_diagnostics(*context));
    row(); EXPECT_FALSE(context->state.timing_file);
    context->state.inbasename = utf8(directory / "録画");
    ASSERT_TRUE(comskip::media::open_timing_diagnostics(*context));
    auto* original = context->state.timing_file.get();
    context->settings.output_timing = false;
    EXPECT_FALSE(comskip::media::open_timing_diagnostics(*context));
    EXPECT_EQ(context->state.timing_file.get(), original);
    row();
    comskip::media::close_timing_diagnostics(*context);
    EXPECT_EQ(contents(), header + audio_row);
}
TEST_F(TimingOutput, ReopeningTruncatesAndExplicitHeaderRetainsLegacyRestartOutput) {
    ASSERT_TRUE(comskip::media::open_timing_diagnostics(*context)); row();
    comskip::media::close_timing_diagnostics(*context);
    ASSERT_TRUE(comskip::media::open_timing_diagnostics(*context));
    comskip::media::write_timing_header(*context); // Existing decoder restart emits a second header.
    comskip::media::close_timing_diagnostics(*context);
    comskip::media::close_timing_diagnostics(*context);
    EXPECT_EQ(contents(), std::string(header) + header);
}
TEST_F(TimingOutput, ExceptionUnwindFlushesOwnedRowsAndReleasesRecording) {
    try {
        auto recording = std::move(context);
        ASSERT_TRUE(comskip::media::open_timing_diagnostics(*recording));
        comskip::media::write_timing_row(*recording, "a frame", 1.25, 0.04, 1.5, 1.25, -0.125, 2);
        throw std::runtime_error("analysis interrupted");
    } catch (const std::runtime_error&) {}
    EXPECT_FALSE(context);
    EXPECT_EQ(contents(), header + audio_row);
    EXPECT_TRUE(std::filesystem::remove(directory / "録画.timing.csv"));
}
}
