#include "settings_value.h"
#include "settings_descriptors.h"
#include <gtest/gtest.h>
#include <set>
#include <limits>

namespace {
using comskip::config::Ini;
using comskip::config::default_settings;
using comskip::config::load_settings;

TEST(SettingsValue, ChecksBrightnessOverridesAndInheritedValues) {
    const auto base = default_settings();
    for (auto key : {"max_brightness", "test_brightness"}) {
        for (int bad : {-1,256})
            EXPECT_THROW(load_settings(Ini(std::string(key)+"="+std::to_string(bad)),base),
                         std::invalid_argument);
        EXPECT_NO_THROW(load_settings(Ini(std::string(key)+"=0"),base));
        EXPECT_NO_THROW(load_settings(Ini(std::string(key)+"=255"),base));
    }
    auto inherited = base;
    inherited.max_brightness = 256;
    EXPECT_THROW(load_settings(Ini(""),inherited),std::invalid_argument);
    inherited = base;
    inherited.test_brightness = -1;
    EXPECT_THROW(load_settings(Ini(""),inherited),std::invalid_argument);
}

TEST(SettingsValue, RejectsInvalidFrameMasksAndAcceptsPercentageLimits) {
    const auto base = default_settings();
    for (const auto key : {"ticker_tape", "top_ticker_tape", "ignore_side",
                           "ignore_left_side", "ignore_right_side"}) {
        EXPECT_THROW(load_settings(Ini(std::string(key) + "=-1"), base), std::invalid_argument)
            << key;
    }
    for (const auto key : {"ticker_tape_percentage", "top_ticker_tape_percentage"}) {
        for (int value : {-1,101})
            EXPECT_THROW(load_settings(Ini(std::string(key) + "=" + std::to_string(value)), base),
                         std::invalid_argument) << key << ": " << value;
        EXPECT_NO_THROW(load_settings(Ini(std::string(key) + "=0"), base));
        EXPECT_NO_THROW(load_settings(Ini(std::string(key) + "=100"), base));
    }
}

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
    EXPECT_EQ(load_settings(Ini("windowtitle=\"" + std::string(1100, 'x') + "\""), base).windowtitle,
        std::string(1100, 'x'));
}
TEST(SettingsValue, RejectsNegativeScanBordersIncludingInheritedValues) {
    const auto baseline = default_settings();
    EXPECT_THROW(load_settings(Ini("border=-1"), baseline), std::invalid_argument);
    EXPECT_EQ(load_settings(Ini("border=0"), baseline).border, 0);
    EXPECT_EQ(load_settings(Ini("border=12"), baseline).border, 12);
    auto invalid = baseline;
    invalid.border = -1;
    EXPECT_THROW(load_settings(Ini{}, invalid), std::invalid_argument);
    EXPECT_EQ(baseline.border, default_settings().border);
}
TEST(SettingsValue, LoadsIndependentFontSettingsAndLongOwnedStringRoundTrips) {
    const auto base = default_settings();
    EXPECT_TRUE(base.review_font_file.empty()); EXPECT_EQ(base.review_font_size, 16);
    const std::string title = std::string(1100, 'x') + " café";
    const Ini input("review_font_file=\"café.ttf\"\nreview_font_size=22\nwindowtitle=\"" + title + "\"");
    const auto value = load_settings(Ini(input.serialize()), base);
    EXPECT_EQ(value.review_font_file, "café.ttf"); EXPECT_EQ(value.review_font_size, 22);
    EXPECT_EQ(value.windowtitle, title);
    EXPECT_EQ(base.review_font_size, 16); EXPECT_TRUE(base.review_font_file.empty());
    for (int size : {0, -1})
        EXPECT_THROW(load_settings(Ini("review_font_size=" + std::to_string(size)), base), std::invalid_argument);
    auto inherited = value; inherited.review_font_size = 0;
    EXPECT_THROW(load_settings(Ini(""), inherited), std::invalid_argument);
}
TEST(SettingsValue, RejectsUnsafeOutputTemplatesBeforeReturningCandidate) {
    const auto base = default_settings();
    for (const std::string key : {"avisynth_options", "dvrcut_options"}) {
        for (const std::string format : {"%n", "%d", "%", "%10s", "%s%s%s%s"}) {
            EXPECT_THROW(load_settings(Ini(key + "=\"" + format + "\""), base), std::invalid_argument)
                << key << ": " << format;
        }
    }
    EXPECT_THROW(load_settings(Ini("avisynth_options=\"%s%s\""), base), std::invalid_argument);
    auto inherited = base;
    inherited.dvrcut_options = "%n";
    EXPECT_THROW(load_settings(Ini{}, inherited), std::invalid_argument);
    const auto valid = load_settings(Ini("avisynth_options=\"%% %s\"\ndvrcut_options=\"%s|%s|%s|%%\""), base);
    EXPECT_EQ(valid.avisynth_options, "%% %s");
    EXPECT_EQ(valid.dvrcut_options, "%s|%s|%s|%%");
    EXPECT_EQ(base.avisynth_options, default_settings().avisynth_options);
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
