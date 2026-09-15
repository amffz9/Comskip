#include "file_resources.h"
#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <type_traits>

using comskip::platform::FilePtr;
static_assert(!std::is_copy_constructible_v<FilePtr>);
static_assert(std::is_nothrow_move_constructible_v<FilePtr>);

TEST(FileResources, MovesOwnershipAndCanResetRepeatedly) {
    auto original = comskip::platform::temporary_file();
    ASSERT_NE(original, nullptr);
    auto* borrowed = original.get();
    FilePtr moved = std::move(original);
    EXPECT_EQ(original, nullptr);
    EXPECT_EQ(moved.get(), borrowed);
    EXPECT_GE(std::fputs("owned", moved.get()), 0);
    moved.reset();
    EXPECT_EQ(moved, nullptr);
    moved.reset();
}
TEST(FileResources, DestructionClosesAndFlushesBufferedOutput) {
    const auto filename = std::filesystem::temp_directory_path() /
        ("comskip-file-owner-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp");
    struct RemoveTemporaryFile {
        std::filesystem::path filename;
        ~RemoveTemporaryFile() { std::error_code ignored; std::filesystem::remove(filename, ignored); }
    } cleanup{filename};
    const std::string expected = "Buffered output survives automatic FILE destruction.";
    {
        std::array<char, 4096> buffer{};
        auto file = comskip::platform::own_file(std::fopen(filename.string().c_str(), "wb"));
        ASSERT_NE(file, nullptr);
        ASSERT_EQ(std::setvbuf(file.get(), buffer.data(), _IOFBF, buffer.size()), 0);
        ASSERT_EQ(std::fwrite(expected.data(), 1, expected.size(), file.get()), expected.size());
    }
    std::ifstream input(filename, std::ios::binary);
    ASSERT_TRUE(input.good());
    EXPECT_EQ((std::string{std::istreambuf_iterator<char>(input), {}}), expected);
}
