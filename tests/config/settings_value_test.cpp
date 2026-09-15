#include "settings_value.h"
#include "settings_descriptors.h"
#include <gtest/gtest.h>
#include <set>
#include <limits>

namespace {
using comskip::config::Ini;
using comskip::config::default_settings;
using comskip::config::load_settings;

TEST(SettingsValue, OwnsEveryCommittedDefault) {
    const auto settings = default_settings();
    EXPECT_EQ(settings.commDetectMethod, 123);
    EXPECT_EQ(settings.thread_count, 2);
    EXPECT_EQ(settings.brightness_jump, 200);
    EXPECT_EQ(settings.windowtitle, "Comskip - %s");
    EXPECT_EQ(settings.language, "en");
    EXPECT_TRUE(settings.locale_directory.empty());
    EXPECT_EQ(settings.commercial_profile.strict_lengths.front(), 10);
    const auto known = comskip::config::configured_setting_keys();
    const std::set<std::string_view> recognized(known.begin(), known.end());
    EXPECT_EQ(recognized.size(), known.size());
    EXPECT_EQ(recognized.size(), comskip::config::defaults().values().size());
    for (const auto& [key, value] : comskip::config::defaults().values())
        EXPECT_TRUE(recognized.contains(key)) << key;
    comskip::config::detail::for_each_setting(settings, [&](std::string_view key, const auto& value, int sign) {
        using T = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>)
            EXPECT_EQ(value, *comskip::config::defaults().find(key)) << key;
        else
            EXPECT_EQ(value, sign * comskip::config::defaults().number<T>(key)) << key;
    });
}
TEST(SettingsValue, KeepsInterleavedRecordingsAndProfilesIndependent) {
    const auto initial = default_settings();
    const auto first = load_settings(Ini("thread_count=4\ncommercial_lengths=18,72\nlanguage=es"), initial);
    const auto second = load_settings(Ini("thread_count=1\ncommercial_lengths=15,30\ncommercial_show_margin=7"), initial);
    const auto changed_first = load_settings(Ini("max_volume=900\ncommercial_minimum_tolerance=0.75"), first);
    EXPECT_EQ(first.thread_count, 4);
    EXPECT_EQ(first.max_volume, 500);
    EXPECT_EQ(changed_first.max_volume, 900);
    EXPECT_EQ(changed_first.commercial_profile.strict_lengths, (std::vector<int>{18, 72}));
    EXPECT_EQ(second.commercial_profile.strict_lengths, (std::vector<int>{15, 30}));
    EXPECT_EQ(second.thread_count, 1);
    EXPECT_EQ(second.language, "en");
    EXPECT_DOUBLE_EQ(second.commercial_profile.show_margin, 7);
    EXPECT_DOUBLE_EQ(first.commercial_profile.minimum_tolerance, 0.5);
    EXPECT_DOUBLE_EQ(changed_first.commercial_profile.minimum_tolerance, 0.75);
}
TEST(SettingsValue, FailureChangesNeitherBaselineNorOtherRecording) {
    const auto baseline = default_settings();
    const auto other = load_settings(Ini("max_volume=700\ncommercial_lengths=18,72"), baseline);
    EXPECT_THROW(load_settings(Ini("max_volume=900\nthread_count=0\ncommercial_lengths=18"), baseline), std::invalid_argument);
    EXPECT_EQ(baseline.max_volume, 500);
    EXPECT_EQ(other.max_volume, 700);
    EXPECT_EQ(other.commercial_profile.strict_lengths, (std::vector<int>{18, 72}));
    const auto success = load_settings(Ini("max_volume=800\ncommercial_lengths=72"), baseline);
    EXPECT_EQ(success.max_volume, 800);
    EXPECT_EQ(other.max_volume, 700);
    EXPECT_EQ(other.commercial_profile.strict_lengths, (std::vector<int>{18, 72}));
}
TEST(SettingsValue, PreservesLegacyValidationAndDelayConvention) {
    const auto base = default_settings();
    const auto value = load_settings(Ini("ms_audio_delay=25\ndiv5_tolerance=-1\nwindowtitle=\"%% %s\""), base);
    EXPECT_EQ(value.ms_audio_delay, -25);
    EXPECT_DOUBLE_EQ(value.div5_tolerance, -1);
    for (const auto* text : {"ms_audio_delay=-2147483648", "fps=0", "output_edl=2",
                            "num_logo_buffers=0", "edge_radius=0", "edge_step=-1",
                            "windowtitle=\"%n\"", "windowtitle=\"%s %s\"", "language=de",
                            "commercial_minimum_tolerance=2\ncommercial_maximum_tolerance=1"})
        EXPECT_THROW(load_settings(Ini(text), base), std::invalid_argument) << text;
    EXPECT_THROW(load_settings(Ini("windowtitle=\"" + std::string(1100, 'x') + "\""), base), std::invalid_argument);
}
TEST(SettingsValue, ValidatesInheritedBaselineAsWellAsOverrides) {
    auto base = default_settings();
    base.global_threshold = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(load_settings(Ini{}, base), std::invalid_argument);
    base = default_settings();
    base.commercial_profile.strict_lengths = {-10};
    EXPECT_THROW(load_settings(Ini{}, base), std::invalid_argument);
}
}
