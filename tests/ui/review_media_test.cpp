#include "ui/review_media.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <random>

namespace {
std::string utf8(const std::filesystem::path& path) {
    const auto value = path.u8string(); return {value.begin(), value.end()};
}
}
TEST(ReviewMedia, ChangesOnlyFinalExtensionIncludingExtensionlessAndDottedDirectories) {
    const auto files = comskip::ui::review_media_candidates("folder.with.dots/recording.mpg");
    EXPECT_EQ(files[0], std::filesystem::path("folder.with.dots/recording.mpg"));
    EXPECT_EQ(files[1], std::filesystem::path("folder.with.dots/recording.ts"));
    EXPECT_EQ(files[2], std::filesystem::path("folder.with.dots/recording.dvr-ms"));
    const auto extensionless = comskip::ui::review_media_candidates("folder.with.dots/recording");
    EXPECT_EQ(extensionless[1], std::filesystem::path("folder.with.dots/recording.ts"));
    EXPECT_EQ(extensionless[2], std::filesystem::path("folder.with.dots/recording.dvr-ms"));
}
TEST(ReviewMedia, NearLegacyCapacityUnicodePathCanGrowWithoutTruncation) {
    const std::string original = std::string(100, 'a') + "/" + std::string(100, 'b') + "/" +
                                 std::string(51, 'c') + "é 字.m";
    ASSERT_GE(original.size(), 259);
    const auto files = comskip::ui::review_media_candidates(original);
    EXPECT_EQ(utf8(files[0]), original);
    const auto base = original.substr(0, original.size() - 2);
    EXPECT_EQ(utf8(files[1]), base + ".ts");
    EXPECT_EQ(utf8(files[2]), base + ".dvr-ms");
    EXPECT_GT(utf8(files[2]).size(), 260);
}
TEST(ReviewMedia, ExistingOriginalThenTransportThenAsfHaveStablePriority) {
    const auto directory = std::filesystem::temp_directory_path() /
        ("comskip review " + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(std::random_device{}()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); } } cleanup{directory};
    const auto original = directory / std::filesystem::path(u8"é 字.mpg");
    const auto files = comskip::ui::review_media_candidates(utf8(original));
    for (const auto& file : files) { std::ofstream output(file); output << "media"; }
    const auto first_readable = [&]() -> std::filesystem::path {
        for (const auto& file : files) if (std::ifstream input(file); input) return file;
        return {};
    };
    EXPECT_EQ(first_readable(), files[0]);
    ASSERT_TRUE(std::filesystem::remove(files[0])); EXPECT_EQ(first_readable(), files[1]);
    ASSERT_TRUE(std::filesystem::remove(files[1])); EXPECT_EQ(first_readable(), files[2]);
    ASSERT_TRUE(std::filesystem::remove(files[2])); EXPECT_TRUE(first_readable().empty());
}
