#include "search_path.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>

namespace {
std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
}
TEST(SearchPath, SearchesWorkingDirectoryBeforeOrderedUnicodeAndQuotedEntries) {
    const auto root = std::filesystem::temp_directory_path() /
        ("comskip-path-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup() {
        std::error_code ignored; std::filesystem::remove_all(path, ignored);
    }} cleanup{root};
    const auto first = root / std::filesystem::path(u8"first café");
    const auto second = root / "second";
    std::filesystem::create_directories(first);
    std::filesystem::create_directory(second);
    { std::ofstream(first / "comskip.ini") << "first"; }
    { std::ofstream(second / "comskip.ini") << "second"; }
    const auto paths = "\"" + utf8(first) + "\";;" + utf8(second);
    EXPECT_EQ(comskip::platform::find_in_search_path("comskip.ini", paths, root, ';'), first / "comskip.ini");
    { std::ofstream(root / "comskip.ini") << "working"; }
    EXPECT_EQ(comskip::platform::find_in_search_path("comskip.ini", paths, root, ';'), root / "comskip.ini");
    EXPECT_FALSE(comskip::platform::find_in_search_path("missing.ini", paths, root, ';'));
    EXPECT_FALSE(comskip::platform::find_in_search_path("", paths, root, ';'));
}
