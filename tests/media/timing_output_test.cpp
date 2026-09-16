#include "recording_context.h"
#include "media/timing_diagnostics.h"
#include "app/analysis.h"
#include "exit_requested.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <cmath>
#include <sstream>
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
    row();
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
TEST_F(TimingOutput, ReopeningTruncatesAndWritesExactlyOneHeaderBeforeValidRows) {
    ASSERT_TRUE(comskip::media::open_timing_diagnostics(*context)); row();
    comskip::media::close_timing_diagnostics(*context);
    ASSERT_TRUE(comskip::media::open_timing_diagnostics(*context));
    row();
    comskip::media::close_timing_diagnostics(*context);
    comskip::media::close_timing_diagnostics(*context);
    EXPECT_EQ(contents(), header + audio_row);
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
TEST_F(TimingOutput, ActualDecoderResetReopensTimingWithOneHeaderAndFlushesOnUnwind) {
    const auto input = directory / "録画.y4m";
    {
        std::ofstream media(input, std::ios::binary);
        media.exceptions(std::ios::failbit | std::ios::badbit);
        media << "YUV4MPEG2 W160 H120 F25:1 Ip A1:1 C420jpeg\n";
        std::vector<char> luma(160 * 120), chroma(luma.size() / 2, static_cast<char>(128));
        for (int frame = 0; frame < 250; ++frame) {
            for (std::size_t pixel = 0; pixel < luma.size(); ++pixel)
                luma[pixel] = static_cast<char>(40 + (pixel + frame) % 160);
            media.write("FRAME\n", 6);
            media.write(luma.data(), static_cast<std::streamsize>(luma.size()));
            media.write(chroma.data(), static_cast<std::streamsize>(chroma.size()));
        }
    }
    const auto settings = directory / "reset.ini";
    {
        std::ofstream ini(settings);
        ini.exceptions(std::ios::failbit | std::ios::badbit);
        ini << "detect_method=1\nnum_logo_buffers=2\noutput_timing=1\nverbose=10\n"
               "output_default=0\noutput_edl=0\noutput_framearray=0\n";
    }
    std::vector<std::string> arguments{"timing-test", "--ini=" + utf8(settings),
        "--output=" + utf8(directory), "--selftest=2", utf8(input)};
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    int status = -99;
    try { status = comskip_main(*context, static_cast<int>(arguments.size()), argv.data()); }
    catch (const comskip::ExitRequested& interrupted) { status = interrupted.status(); }
    ASSERT_EQ(status, 1);
    ASSERT_GT(context->state.pass, 0);
    const double previous_pts = context->state.video_packet_process_prev_pts;
    ASSERT_GT(previous_pts, 0.5); // The reset happened after decoding its first pass.
    context.reset(); // Own every output through exception unwind and recording teardown.
    const auto csv = contents();
    ASSERT_TRUE(csv.starts_with(header));
    ASSERT_EQ(csv.find("sep=,", 1), std::string::npos);
    ASSERT_EQ(csv.find("type   ,", csv.find('\n') + 2), std::string::npos);
    std::istringstream rows(csv.substr(std::char_traits<char>::length(header)));
    std::string line;
    int count = 0;
    while (std::getline(rows, line)) {
        for (char& character : line) if (character == ',') character = ' ';
        std::istringstream fields(line);
        std::string medium, kind;
        double real, step, pts, clock, delta, offset;
        int repeat;
        ASSERT_TRUE(fields >> medium >> kind >> real >> step >> pts >> clock >> delta >> offset >> repeat);
        EXPECT_EQ(medium, "v");
        EXPECT_TRUE(kind == "set" || kind == "clock");
        EXPECT_TRUE(std::isfinite(real)); EXPECT_TRUE(std::isfinite(step));
        EXPECT_NEAR(delta, pts - clock, 0.002);
        EXPECT_NEAR(pts, 0, 0.001); // Selftest exits on the first decoded frame after reset.
        EXPECT_NEAR(step, pts - previous_pts, 0.001); // The diagnostic records the backward reset step.
        ++count;
    }
    EXPECT_GT(count, 0);
    std::ifstream log(directory / "録画.log");
    ASSERT_TRUE(log);
    const std::string diagnostics{std::istreambuf_iterator<char>(log), {}};
    EXPECT_TRUE(diagnostics.contains("Selftest 2 OK: Reset"));
    log.close();
    EXPECT_TRUE(std::filesystem::remove(input));
    EXPECT_TRUE(std::filesystem::remove(directory / "録画.timing.csv"));
}
}
