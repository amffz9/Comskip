#include "platform.h"
#include <gtest/gtest.h>
#include <chrono>
#include <ctime>
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

TEST(PlatformFiles, CppOpenFileAcceptsStringViewsAndRejectsEmbeddedNulls) {
    const auto filename = std::filesystem::temp_directory_path() / "comskip-open-file-test.txt";
    const auto encoded_path = filename.u8string();
    const auto encoded = std::string(reinterpret_cast<const char*>(encoded_path.c_str()), encoded_path.size());
    auto file = comskip::platform::open_file(encoded, "wb");
    ASSERT_NE(file, nullptr);
    std::fputs("ok", file);
    ASSERT_EQ(std::fclose(file), 0);
    const std::string invalid = encoded + '\0';
    errno = 0;
    EXPECT_EQ(comskip::platform::open_file(invalid, "rb"), nullptr);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_EQ(myremove(encoded.c_str()), 0);
}

TEST(PlatformTime, ConvertsCurrentTimeWithoutUsingSharedStorage) {
    std::tm local{};
    ASSERT_TRUE(comskip::platform::local_time(std::time(nullptr), local));
    EXPECT_GE(local.tm_year, 70);
    EXPECT_LT(local.tm_mon, 12);
    EXPECT_GE(local.tm_mday, 1);
    EXPECT_LE(local.tm_mday, 31);
}

TEST(PlatformTime, ReturnsAnOwnedCtimeCompatibleString) {
    const auto text = comskip::platform::time_string(std::time(nullptr));
    ASSERT_FALSE(text.empty());
    EXPECT_EQ(text.back(), '\n');
}
