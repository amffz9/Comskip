#include "recording_context.h"
#include "app/analysis.h"
#include "exit_requested.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>


namespace {
std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
std::filesystem::path native_path(const std::filesystem::path& path) {
#ifdef _WIN32
    return std::filesystem::path(L"\\\\?\\" + std::filesystem::absolute(path).wstring());
#else
    return path;
#endif
}
}
TEST(CliPaths, LongUnicodeInputSettingsAndOutputPathsProduceCompleteExports) {
    const auto root = native_path(std::filesystem::temp_directory_path() /
        ("comskip-cli-paths-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(std::random_device{}())));
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    const auto deep = [&](const char* category) {
        auto path = root / category;
        for (int i = 0; i < 12; ++i) path /= std::string(90, static_cast<char>('a' + i));
        path /= std::filesystem::path(u8"Café 字幕");
        EXPECT_TRUE(std::filesystem::create_directories(path));
        EXPECT_GT(utf8(path).size(), 1024u);
        return path;
    };
    const auto media = deep("input") / std::filesystem::path(u8"Épisode 字幕.y4m");
    const auto ini = deep("settings") / std::filesystem::path(u8"réglages.ini");
    const auto destination = deep("output");
    {
        std::ofstream output(media, std::ios::binary);
        ASSERT_TRUE(output);
        output << "YUV4MPEG2 W160 H120 F25:1 Ip A1:1 C420jpeg\n";
        const std::string luma(160 * 120, 80), chroma(160 * 120 / 2, static_cast<char>(128));
        for (int i = 0; i < 150; ++i) output << "FRAME\n" << luma << chroma;
        ASSERT_TRUE(output);
    }
    {
        std::ofstream settings(ini);
        ASSERT_TRUE(settings);
        settings << "detect_method=17\nnum_logo_buffers=2\noutput_framearray=1\noutput_edl=1\nccCheck=1\n"
                    "output_dvrmstb=1\noutput_plist_cutlist=1\nlive_tv_retries=0\nadded_recording=0\nverbose=0\n";
    }
    auto context = std::make_unique<RecordingContext>();
    std::vector<std::string> arguments{"cli-paths", "--ini=" + utf8(ini), "--output=" + utf8(destination),
        "--threads=1", utf8(media)};
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    int status = -1;
    try { status = comskip_main(*context, static_cast<int>(arguments.size()), argv.data()); }
    catch (const comskip::ExitRequested& exit) { status = exit.status(); }
    EXPECT_TRUE(status == 0 || status == 1);
    EXPECT_EQ(context->state.mpegfilename, utf8(media));
    EXPECT_EQ(context->state.frame_count, 150);
    const auto marker = destination / (media.stem().native() + std::filesystem::path(".ccno").native());
    EXPECT_TRUE(std::filesystem::is_regular_file(marker));
    for (const char* extension : {".txt", ".edl", ".csv", ".xml", ".plist"}) {
        auto file = destination / media.stem(); file += extension;
        ASSERT_TRUE(std::filesystem::is_regular_file(file)) << utf8(file);
        std::ifstream input(file);
        ASSERT_TRUE(input);
        const std::string content(std::istreambuf_iterator<char>(input), {});
        if (std::string{extension} == ".csv")
            EXPECT_EQ(std::count(content.begin(), content.end(), '\n'), 152);
    }
    context.reset();
    // Exercise the real optional marker creation error: output exports remain
    // writable, but a directory at .ccno makes the marker's fopen fail.
    ASSERT_TRUE(std::filesystem::remove(marker));
    ASSERT_TRUE(std::filesystem::create_directory(marker));
    auto failed_marker = std::make_unique<RecordingContext>();
    try { status = comskip_main(*failed_marker, static_cast<int>(arguments.size()), argv.data()); }
    catch (const comskip::ExitRequested& exit) { status = exit.status(); }
    EXPECT_TRUE(status == 0 || status == 1);
    auto log_name = destination / media.stem(); log_name += ".log";
    std::ifstream log(log_name);
    ASSERT_TRUE(log);
    const std::string messages(std::istreambuf_iterator<char>(log), {});
    EXPECT_NE(messages.find("could not create file " + utf8(marker)), std::string::npos);
    failed_marker.reset();
    EXPECT_TRUE(std::filesystem::remove(media));
    EXPECT_TRUE(std::filesystem::remove(ini));
}
