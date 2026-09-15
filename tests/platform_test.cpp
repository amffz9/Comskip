#include "platform.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <string>

TEST(PlatformFiles, OpensReadsAndRemovesUnicodePaths) {
    namespace fs = std::filesystem;
    const auto directory = fs::temp_directory_path() / ("comskip-files-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(fs::create_directory(directory));
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code error; fs::remove(path, error); } } cleanup{directory};
    const auto path = directory / fs::path(u8"\u7535\u89c6 sample.txt");
    const auto encoded = path.u8string();
    const auto* filename = reinterpret_cast<const char*>(encoded.c_str());
    FILE* file = myfopen(filename, "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(std::fwrite("sample", 1, 6, file), 6);
    ASSERT_EQ(std::fclose(file), 0);
    file = myfopen(filename, "rb");
    ASSERT_NE(file, nullptr);
    char contents[7]{};
    EXPECT_EQ(std::fread(contents, 1, 6, file), 6);
    EXPECT_EQ(std::fclose(file), 0);
    EXPECT_STREQ(contents, "sample");
    EXPECT_EQ(myremove(filename), 0);
    EXPECT_FALSE(fs::exists(path));
    EXPECT_EQ(myremove(filename), -1);
    EXPECT_EQ(errno, ENOENT);
}

TEST(PlatformFiles, RejectsNullPathsWithErrno) {
    EXPECT_EQ(myfopen(nullptr, "rb"), nullptr);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_EQ(myremove(nullptr), -1);
    EXPECT_EQ(errno, EINVAL);
}
