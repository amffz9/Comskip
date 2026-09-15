#include "settings.h"
#include <gtest/gtest.h>
using namespace comskip::config;

TEST(Ini, MatchesWholeKeysAndIgnoresComments) {
    Ini ini("; fps=99\n[Main Settings]\nnot_fps=9\nfps = 29.97 ; comment\n#thread_count=99\n");
    EXPECT_DOUBLE_EQ(ini.number<double>("fps"), 29.97);
    EXPECT_EQ(ini.find("thread_count"), nullptr);
}
TEST(Ini, SupportsBomCrLfQuotesEscapesAndLastDuplicate) {
    Ini ini("\xef\xbb\xbf[Main]\r\nx=1\r\nx=2\r\npath=\"C:\\\\TV\\\\clip.ts\"\r\ntext=\"line\\nquote\\\";#\"\n");
    EXPECT_EQ(ini.number<int>("x"), 2);
    ASSERT_NE(ini.find("path"), nullptr);
    EXPECT_EQ(*ini.find("path"), "C:\\TV\\clip.ts");
    EXPECT_EQ(Ini(ini.serialize()).values(), ini.values());
}
TEST(Ini, RejectsMalformedAndNonfiniteNumbers) {
    for (const auto* value : {"nan", "inf", "1e999", "2garbage", ""}) {
        Ini ini(std::string("x=") + value);
        EXPECT_THROW(ini.number<double>("x"), std::invalid_argument);
    }
    EXPECT_THROW(Ini("x=\"unterminated"), std::invalid_argument);
    EXPECT_THROW(Ini("=2"), std::invalid_argument);
    EXPECT_THROW(Ini("x=2.5").number<int>("x"), std::invalid_argument);
    EXPECT_THROW(Ini("x=9999999999999").number<int>("x"), std::invalid_argument);
    EXPECT_THROW(Ini("x=-1").number<unsigned>("x"), std::invalid_argument);
}
class Settings : public testing::Test {
    void SetUp() override { apply_settings(defaults()); }
    void TearDown() override { apply_settings(defaults()); }
};
TEST_F(Settings, LoadsCommittedDefaultsAndPreservesIntegerThresholds) {
    EXPECT_EQ(brightness_jump, 200);
    EXPECT_EQ(thread_count, 2);
    EXPECT_EQ(commDetectMethod, 123);
    EXPECT_DOUBLE_EQ(div5_tolerance, -1);
    EXPECT_STREQ(windowtitle, "Comskip - %s");
    apply_settings(Ini("brightness_jump=150\nmax_volume=800"));
    EXPECT_EQ(brightness_jump, 150);
    EXPECT_EQ(max_volume, 800);
    EXPECT_EQ(thread_count, 2);
}
TEST_F(Settings, SupportsNegativeOverridesAndAudioDelayConvention) {
    apply_settings(Ini("div5_tolerance=0.8\nplay_nice_start=600"));
    apply_settings(Ini("div5_tolerance=-1\nplay_nice_start=-1\nms_audio_delay=25"));
    EXPECT_DOUBLE_EQ(div5_tolerance, -1);
    EXPECT_EQ(play_nice_start, -1);
    EXPECT_EQ(ms_audio_delay, -25);
}
TEST_F(Settings, RejectsOversizedStringsAndInvalidValuesAtomically) {
    EXPECT_THROW(apply_settings(Ini("max_volume=100\nwindowtitle=\"" + std::string(1100, 'x') + "\"")), std::invalid_argument);
    EXPECT_EQ(max_volume, 500);
    EXPECT_THROW(apply_settings(Ini("num_logo_buffers=0")), std::invalid_argument);
    EXPECT_THROW(apply_settings(Ini("thread_count=-1")), std::invalid_argument);
    EXPECT_THROW(apply_settings(Ini("output_edl=2")), std::invalid_argument);
    EXPECT_THROW(apply_settings(Ini("windowtitle=\"%n\"")), std::invalid_argument);
    EXPECT_THROW(apply_settings(Ini("windowtitle=\"%s %s\"")), std::invalid_argument);
    apply_settings(Ini("windowtitle=\"" + std::string(300, 'x') + "\""));
    EXPECT_EQ(std::string(windowtitle).size(), 300u);
}
