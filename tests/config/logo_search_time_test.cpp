#include "logo_search_time.h"
#include "settings_value.h"
#include <gtest/gtest.h>
#include <limits>

using comskip::config::adjusted_logo_search_seconds;

TEST(LogoSearchTime, PreservesOrdinaryExtensionAndNoExtension) {
    EXPECT_EQ(adjusted_logo_search_seconds(30, 2), 150);
    EXPECT_EQ(adjusted_logo_search_seconds(120, 2), 120);
    EXPECT_EQ(adjusted_logo_search_seconds(600, 2), 600);
    EXPECT_EQ(adjusted_logo_search_seconds(30, 0), 30);
    EXPECT_EQ(adjusted_logo_search_seconds(30, -1), 30);
}
TEST(LogoSearchTime, RejectsMultiplicationAndAdditionBeyondSecondsStorage) {
    constexpr int maximum = std::numeric_limits<int>::max();
    EXPECT_THROW(adjusted_logo_search_seconds(0, maximum), std::out_of_range);
    EXPECT_THROW(adjusted_logo_search_seconds(maximum / 2, maximum / 60), std::out_of_range);
    EXPECT_EQ(adjusted_logo_search_seconds(0, maximum / 60), (maximum / 60) * 60);
}
TEST(LogoSearchTime, SettingsRejectInvalidExtensionWithoutChangingBaseline) {
    const auto baseline = comskip::config::default_settings();
    EXPECT_THROW(comskip::config::load_settings(comskip::config::Ini(
        "added_recording=2147483647"), baseline), std::out_of_range);
}
