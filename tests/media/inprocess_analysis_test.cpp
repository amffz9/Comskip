#include "exit_requested.h"
#include "recording_context.h"
#include "app/analysis.h"
#include "localization/diagnostic.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>


namespace {
std::string utf8(const std::filesystem::path& path)
{
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

class Workspace {
    std::filesystem::path path_;
public:
    Workspace()
    {
        const auto sequence = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("comskip inprocess " + std::to_string(sequence) + "-" + std::to_string(std::random_device{}()));
        if (!std::filesystem::create_directory(path_)) throw std::runtime_error("Cannot create in-process test workspace");
    }
    ~Workspace() { std::error_code ignored; std::filesystem::remove_all(path_, ignored); }
    const auto& path() const { return path_; }
    void verify_cleanup()
    {
        // On Windows this also exposes input/output handles retained after the
        // context dies. Unix handle accounting below detects unlinked open files.
        std::filesystem::remove_all(path_);
        if (std::filesystem::exists(path_)) throw std::runtime_error("Analysis retained its test workspace");
    }
};

void generate_fixture(const std::filesystem::path& filename, int width, int height)
{
    std::ofstream output(filename, std::ios::binary);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << "YUV4MPEG2 W" << width << " H" << height << " F25:1 Ip A1:1 C420jpeg\n";
    std::vector<char> pixels(static_cast<std::size_t>(width) * height);
    const std::vector<char> chroma(pixels.size() / 2, static_cast<char>(128));
    for (int frame = 0; frame < 250; ++frame) {
        for (std::size_t index = 0; index < pixels.size(); ++index)
            pixels[index] = static_cast<char>(frame % 100 < 4 ? 16 : 40 + (index + frame) % 160);
        output.write("FRAME\n", 6);
        output.write(pixels.data(), static_cast<std::streamsize>(pixels.size()));
        output.write(chroma.data(), static_cast<std::streamsize>(chroma.size()));
    }
}

std::string read_file(const std::filesystem::path& filename)
{
    std::ifstream input(filename, std::ios::binary);
    if (!input) throw std::runtime_error("Missing analysis output: " + utf8(filename));
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct Result { std::string csv; std::string edl; std::string cutlist; };

Result analyze(const std::filesystem::path& fixture, const std::filesystem::path& root,
               std::string_view label, int threads, int offset, bool china, std::string_view language)
{
    const auto destination = root / label;
    std::filesystem::create_directory(destination);
    const auto ini = destination / "settings.ini";
    {
        std::ofstream settings(ini);
        settings.exceptions(std::ios::failbit | std::ios::badbit);
        settings << "detect_method=1\nnum_logo_buffers=2\noutput_framearray=1\noutput_edl=1\n"
                    "live_tv_retries=0\nadded_recording=0\nverbose=0\n"
                 << "edl_offset=" << offset << "\n";
        if (china) settings << "commercial_lengths=10,15,18,20,25,30,36,45,60,72,90,108,120,126,150,180\n";
    }
    std::vector<std::string> arguments{"comskip-inprocess", "--ini=" + utf8(ini),
        "--threads=" + std::to_string(threads), "--output=" + utf8(destination),
        "--language=" + std::string(language), utf8(fixture)};
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    {
        auto context = std::make_unique<RecordingContext>();
        EXPECT_EQ(context->state.frame_count, 0);
        EXPECT_EQ(context->state.ac3_packet_index, 0);
        EXPECT_EQ(context->state.video_packet_process_prev_pts, 0.0);
        EXPECT_EQ(context->settings.edl_offset, 0);
        int status;
        try { status = comskip_main(*context, static_cast<int>(arguments.size()), argv.data()); }
        catch (const comskip::ExitRequested& request) { status = request.status(); }
        EXPECT_TRUE(status == 0 || status == 1) << "Analysis failed for " << label << " with " << status;
        EXPECT_EQ(context->settings.thread_count, threads);
        EXPECT_EQ(context->settings.edl_offset, offset);
        EXPECT_EQ(context->state.frame_count, 250);
        EXPECT_EQ(context->state.commercial_count, 0);
        EXPECT_EQ(context->state.commercial[0].end_frame, 250);
    }
    const auto basename = destination / fixture.stem();
    return {read_file(basename.string() + ".csv"), read_file(basename.string() + ".edl"),
            read_file(basename.string() + ".txt")};
}

std::optional<std::uint64_t> process_handles()
{
#ifdef _WIN32
    DWORD handles{};
    if (!GetProcessHandleCount(GetCurrentProcess(), &handles)) throw std::runtime_error("Cannot inspect test process handles");
    return handles;
#elif defined(__linux__) || defined(__APPLE__)
    const auto directory = std::filesystem::path(
#ifdef __linux__
        "/proc/self/fd"
#else
        "/dev/fd"
#endif
    );
    return static_cast<std::uint64_t>(std::distance(std::filesystem::directory_iterator(directory),
                                                  std::filesystem::directory_iterator{}));
#else
    return {};
#endif
}

void expect_same(const Result& actual, const Result& expected, std::string_view label)
{
    EXPECT_EQ(actual.edl, expected.edl) << label;
    EXPECT_EQ(actual.cutlist, expected.cutlist) << label;
    EXPECT_TRUE(actual.csv == expected.csv) << label << " CSV differed; expected " << expected.csv.size()
                                          << " bytes, got " << actual.csv.size();
}

void expect_failure(const std::filesystem::path& fixture, const std::filesystem::path& ini,
                    const std::filesystem::path& destination, int expected_status)
{
    std::filesystem::create_directory(destination);
    std::vector<std::string> arguments{"comskip-inprocess", "--ini=" + utf8(ini),
        "--output=" + utf8(destination), utf8(fixture)};
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    auto context = std::make_unique<RecordingContext>();
    bool exit_requested = false;
    try { comskip_main(*context, static_cast<int>(arguments.size()), argv.data()); }
    catch (const comskip::ExitRequested& request) {
        exit_requested = true;
        EXPECT_EQ(request.status(), expected_status);
    }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        exit_requested = true;
        EXPECT_EQ(expected_status, -1);
        EXPECT_EQ(error.diagnostic().code,
                  comskip::diagnostics::Code::cannot_open_recording_detail);
    }
    EXPECT_TRUE(exit_requested) << "Failed analysis did not report its error to the application boundary";
    EXPECT_EQ(context->state.frame_count, 0);
}
}

TEST(InprocessAnalysis, IndependentRecordingContextsProduceExactResultsWithInterleavedSettings)
{
    Workspace workspace;
    const auto first = workspace.path() / "sample.y4m";
    const auto second = workspace.path() / "other.y4m";
    generate_fixture(first, 160, 120);
    generate_fixture(second, 192, 144);
    const auto baseline = analyze(first, workspace.path(), "baseline", 1, 0, false, "en");
    EXPECT_EQ(baseline.edl, "0.00\t9.92\t0\n");
    const auto shifted = analyze(second, workspace.path(), "shifted", 4, 10, true, "es");
    // The final cut ends at frame_count (250). EDL subtracts its
    // ten-frame offset before timestamp lookup, giving frame[240].pts = 9.56.
    // Without an offset the out-of-range end clamps to frame[249].pts = 9.92.
    EXPECT_EQ(shifted.edl, "0.00\t9.56\t0\n");
    // The first run permits one-time library initialization. Subsequent complete
    // runs must release their threads, media handles, and output descriptors.
    const auto warmed_handles = process_handles();
    expect_same(analyze(first, workspace.path(), "first-repeat", 4, 0, false, "es"), baseline, "first-repeat");
    expect_same(analyze(second, workspace.path(), "second-repeat", 1, 10, true, "en"), shifted, "second-repeat");
    expect_same(analyze(first, workspace.path(), "first-final", 1, 0, false, "en"), baseline, "first-final");
    if (warmed_handles) EXPECT_EQ(process_handles(), warmed_handles) << "Independent analyses retained process handles";
    EXPECT_NO_THROW(workspace.verify_cleanup());
}

TEST(InprocessAnalysis, FailedAnalysesUnwindResourcesAndLeaveTheNextRecordingIndependent)
{
    Workspace workspace;
    const auto fixture = workspace.path() / "sample.y4m";
    generate_fixture(fixture, 160, 120);
    const auto baseline = analyze(fixture, workspace.path(), "baseline", 1, 0, false, "en");
    const auto warmed_handles = process_handles();
    const auto malformed_ini = workspace.path() / "malformed.ini";
    {
        std::ofstream settings(malformed_ini);
        settings.exceptions(std::ios::failbit | std::ios::badbit);
        settings << "num_logo_buffers=0\nlive_tv_retries=0\n";
    }
    expect_failure(fixture, malformed_ini, workspace.path() / "bad-config", 1);
    if (warmed_handles) EXPECT_EQ(process_handles(), warmed_handles) << "Malformed settings retained process handles";

    const auto valid_ini = workspace.path() / "valid.ini";
    {
        std::ofstream settings(valid_ini);
        settings.exceptions(std::ios::failbit | std::ios::badbit);
        settings << "live_tv_retries=0\n";
    }
    expect_failure(workspace.path() / "missing.y4m", valid_ini, workspace.path() / "missing-input", -1);
    if (warmed_handles) EXPECT_EQ(process_handles(), warmed_handles) << "Missing input retained process handles";
    expect_same(analyze(fixture, workspace.path(), "after-failures", 4, 0, false, "es"), baseline, "after-failures");
    if (warmed_handles) EXPECT_EQ(process_handles(), warmed_handles) << "Analysis after failures retained process handles";
    EXPECT_NO_THROW(workspace.verify_cleanup());
}

TEST(InprocessAnalysis, ReplaysCsvAndReleasesItsConsumedInput)
{
    Workspace workspace;
    const auto fixture = workspace.path() / "sample.y4m";
    generate_fixture(fixture, 160, 120);
    const auto baseline = analyze(fixture, workspace.path(), "baseline", 1, 0, false, "en");
    const auto handles = process_handles();
    const auto output = workspace.path() / "replay";
    std::filesystem::create_directory(output);
    std::vector<std::string> arguments{"comskip-inprocess",
        "--ini=" + utf8(workspace.path() / "baseline" / "settings.ini"),
        "--output=" + utf8(output), utf8(workspace.path() / "baseline" / "sample.csv")};
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    {
        auto context = std::make_unique<RecordingContext>();
        int status = -99;
        try { status = comskip_main(*context, static_cast<int>(arguments.size()), argv.data()); }
        catch (const comskip::ExitRequested& request) { status = request.status(); }
        EXPECT_EQ(status, 0);
        EXPECT_EQ(context->state.in_file, nullptr);
    }
    EXPECT_EQ(read_file(output / "sample.edl"), baseline.edl);
    EXPECT_EQ(read_file(output / "sample.txt"), baseline.cutlist);
    if (handles) EXPECT_EQ(process_handles(), handles);
    EXPECT_NO_THROW(workspace.verify_cleanup());
}
