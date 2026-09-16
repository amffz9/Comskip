#include "review_window.h"
#include "bundled_font.h"
#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <fstream>
#include <chrono>

TEST(ReviewFont, DefaultFontRendersAfterRelocation) {
    using namespace comskip::ui;
    WindowOptions options{160, 120, "Review café"};
    options.hidden = true;
    ASSERT_TRUE(options.font_path.empty());
    ReviewWindow first(options), second(options);
    const std::vector<std::uint8_t> pixels(160 * 120 * 3, 127);
    const std::array<std::string_view, 2> help{"Review café", "Brightness and timing"};
    for (int pass = 0; pass < 2; ++pass) {
        first.open(); second.open();
        first.draw(pixels); second.draw(pixels);
        first.show_help(help); second.show_details("Frame 25\nCafé");
        EXPECT_EQ(first.overlay_text(), "Review café\nBrightness and timing");
        EXPECT_EQ(second.overlay_text(), "Frame 25\nCafé");
        first.close();
        EXPECT_TRUE(second.is_open());
        second.show_details("Independent window");
        second.close();
    }
}
TEST(ReviewFont, ExternalUnicodeOverrideOpensAndInvalidOverrideDoesNotFallBack) {
    using namespace comskip::ui;
    const auto utf8_path = [](std::string_view value) {
        return std::filesystem::path(std::u8string(value.begin(), value.end()));
    };
    const auto directory = std::filesystem::temp_directory_path() /
        utf8_path("comskip-font-café-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup { std::filesystem::path path; ~Cleanup() {
        std::error_code error; std::filesystem::remove_all(path, error);
    }} cleanup{directory};
    const auto path = directory / std::filesystem::path(u8"café.ttf");
    const auto bytes = bundled_font();
    { std::ofstream file(path, std::ios::binary);
      file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
      ASSERT_TRUE(file.good()); }
    WindowOptions options{160, 120}; options.hidden = true;
    options.font_path = path; options.font_size = 20;
    ReviewWindow window(options);
    window.open(); window.show_details("External café font");
    EXPECT_THROW(window.configure_font({}, 16), std::logic_error);
    window.close();
    window.configure_font(directory / "missing.ttf", 20);
    EXPECT_THROW(window.open(), std::runtime_error);
    EXPECT_FALSE(window.is_open());
    window.configure_font({}, 16);
    EXPECT_NO_THROW(window.open());
    window.close();
}
