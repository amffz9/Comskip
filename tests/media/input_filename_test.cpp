#include "recording_context.h"
#include "media/decoder.h"
#include "media/video_state.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>


namespace {
std::string utf8(const std::filesystem::path& value) {
    const auto bytes = value.u8string(); return {bytes.begin(), bytes.end()};
}
std::filesystem::path native_test_path(const std::filesystem::path& path) {
#ifdef _WIN32
    // The standard filesystem accepts Windows extended-length path syntax;
    // explicit syntax avoids dependence on the host's long-path registry flag.
    return std::filesystem::path(L"\\\\?\\" + std::filesystem::absolute(path).wstring());
#else
    return path;
#endif
}
}
TEST(InputFilename, LongNestedUnicodeMediaOpensAndDemuxesAllPacketsWithoutFixedBuffers) {
    const auto root = std::filesystem::temp_directory_path() /
        ("comskip-input-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(std::random_device{}()));
    auto native_root = native_test_path(root);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } cleanup{native_root};
    auto nested = native_root;
    constexpr int path_segments =
#ifdef __APPLE__
        8;
#else
        12;
#endif
    for (int i = 0; i < path_segments; ++i) nested /= std::string(90, static_cast<char>('a' + i));
    ASSERT_TRUE(std::filesystem::create_directories(nested));
    const auto media = nested / std::filesystem::path(u8"Café 字幕.y4m");
    const auto name = utf8(media);
#ifdef __APPLE__
    ASSERT_GT(name.size(), 260);
#else
    ASSERT_GT(name.size(), 1024);
#endif
    {
        std::ofstream output(media, std::ios::binary);
        ASSERT_TRUE(output);
        output << "YUV4MPEG2 W160 H120 F25:1 Ip A1:1 C420jpeg\n";
        const std::string luma(160 * 120, 80), chroma(160 * 120 / 2, static_cast<char>(128));
        for (int i = 0; i < 10; ++i) output << "FRAME\n" << luma << chroma;
        ASSERT_TRUE(output);
    }
    auto first = std::make_unique<RecordingContext>(), second = std::make_unique<RecordingContext>();
    first->state.mpegfilename = name;
    second->state.mpegfilename = "independent.mpg";
    EXPECT_NO_THROW(file_open(*first));
    ASSERT_TRUE(first->state.video_owner);
    auto& video = *first->state.video_owner;
    ASSERT_TRUE(video.pFormatCtx);
    EXPECT_EQ(video.filename, name);
    EXPECT_EQ(first->state.mpegfilename, name);
    EXPECT_EQ(second->state.mpegfilename, "independent.mpg");
    auto packet = comskip::media::make_packet();
    int frames = 0;
    while (av_read_frame(video.pFormatCtx.get(), packet.get()) >= 0) {
        if (video.videoStream && packet->stream_index == *video.videoStream) ++frames;
        av_packet_unref(packet.get());
    }
    EXPECT_EQ(frames, 10);
    file_close(*first);
    EXPECT_TRUE(std::filesystem::remove(media));
}
