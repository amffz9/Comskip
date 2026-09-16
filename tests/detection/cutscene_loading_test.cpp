#include "recording_context.h"
#include "checked_format.h"
#include "platform/utf8_paths.h"
#include "exit_requested.h"
#include <iomanip>
#include <limits>
#include <gtest/gtest.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <iterator>
#include <random>
#include <vector>

namespace {
class CutsceneLoading : public ::testing::Test {
protected:
    std::unique_ptr<RecordingContext> context;
    std::filesystem::path directory;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip cutscene " + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 1;
        comskip::checked_format(context->state.logfilename, "%s", (directory / "test.log").string().c_str());
    }
    void TearDown() override { context.reset(); std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    std::filesystem::path record(const char* name, int brightness, const std::vector<unsigned char>& pixels,
                                 std::size_t header_bytes = sizeof(int)) {
        auto path = directory / name;
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(&brightness), static_cast<std::streamsize>(header_bytes));
        output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
        return path;
    }
    void load(const std::filesystem::path& path) { const auto name = path.string(); LoadCutScene(*context, name.c_str()); }
};
}
TEST_F(CutsceneLoading, EightRealFilesFillCapacityAndNinthCannotOverwriteAnyRecord) {
    for (int i = 0; i < 8; ++i) {
        const auto name = "scene" + std::to_string(i) + ".cut";
        load(record(name.c_str(), 40 + i, {static_cast<unsigned char>(i), 90, 120}));
        ASSERT_EQ(context->state.cutscenes, i + 1);
        EXPECT_EQ(context->state.csbrightness[i], 40 + i);
        EXPECT_EQ(context->state.cslength[i], 3);
        EXPECT_EQ(context->state.cutscene[i][0], i);
    }
    std::array<std::vector<unsigned char>, 8> before;
    for (int i = 0; i < 8; ++i) before[i].assign(std::begin(context->state.cutscene[i]), std::end(context->state.cutscene[i]));
    const auto ninth = record("ninth.cut", 250, {255, 254, 253});
    EXPECT_NO_THROW(load(ninth));
    EXPECT_EQ(context->state.cutscenes, 8);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(context->state.csbrightness[i], 40 + i);
        EXPECT_EQ(context->state.cslength[i], 3);
        EXPECT_TRUE(std::equal(before[i].begin(), before[i].end(), std::begin(context->state.cutscene[i])));
    }
    EXPECT_FALSE(context->state.cutscene_file);
    EXPECT_TRUE(std::filesystem::remove(ninth));
}
TEST_F(CutsceneLoading, EveryPartialBrightnessHeaderAndMissingPayloadLeaveStateUnchanged) {
    context->state.csbrightness[0] = 0x12345678;
    context->state.cslength[0] = 17;
    context->state.cutscene[0][0] = 55;
    for (std::size_t length = 0; length <= sizeof(int); ++length) {
        const auto path = record("partial.cut", -1, {}, length);
        EXPECT_NO_THROW(load(path));
        EXPECT_EQ(context->state.cutscenes, 0);
        EXPECT_EQ(context->state.csbrightness[0], 0x12345678);
        EXPECT_EQ(context->state.cslength[0], 17);
        EXPECT_EQ(context->state.cutscene[0][0], 55);
        EXPECT_TRUE(std::filesystem::remove(path));
    }
    load(record("valid.cut", 75, {42, 43}));
    EXPECT_EQ(context->state.cutscenes, 1);
    EXPECT_EQ(context->state.csbrightness[0], 75);
    EXPECT_EQ(context->state.cslength[0], 2);
}
TEST_F(CutsceneLoading, OversizedPixelPayloadIsRejectedAndMaximumPayloadIsPreserved) {
    constexpr std::size_t capacity = sizeof(context->state.cutscene[0]);
    load(record("oversized.cut", 70, std::vector<unsigned char>(capacity + 1, 88)));
    EXPECT_EQ(context->state.cutscenes, 0);
    EXPECT_EQ(context->state.csbrightness[0], 0);
    EXPECT_EQ(context->state.cutscene[0][0], 0);
    load(record("maximum.cut", 80, std::vector<unsigned char>(capacity, 99)));
    EXPECT_EQ(context->state.cutscenes, 1);
    EXPECT_EQ(context->state.cslength[0], capacity);
    EXPECT_EQ(context->state.cutscene[0][capacity - 1], 99);
}

TEST_F(CutsceneLoading, ActualIniLoadingAcceptsLongUnicodeCutscenePath) {
#ifdef _WIN32
    directory = std::filesystem::path(L"\\\\?\\" + std::filesystem::absolute(directory).wstring());
#endif
    auto deep = directory;
    for (int index = 0; index < 12; ++index)
        deep /= std::string(90, static_cast<char>('a' + index));
    deep /= std::filesystem::path(u8"Café 字幕");
    ASSERT_TRUE(std::filesystem::create_directories(deep));
    const auto scene = deep / std::filesystem::path(u8"Scène.cut");
    const auto name = comskip::platform::path_to_utf8(scene);
    ASSERT_GT(name.size(), 1024u);
    {
        std::ofstream output(scene, std::ios::binary);
        ASSERT_TRUE(output);
        const int brightness = 75;
        output.write(reinterpret_cast<const char*>(&brightness), sizeof brightness);
        output.write("ABC", 3);
    }
    const auto settings = directory / "settings.ini";
    {
        std::ofstream output(settings);
        ASSERT_TRUE(output);
        output << "cutscenefile1=" << std::quoted(name) << "\nverbose=0\n";
    }
    context->state.ini_file.reset(myfopen(comskip::platform::path_to_utf8(settings).c_str(), "r"));
    ASSERT_TRUE(context->state.ini_file);
    EXPECT_NO_THROW(LoadIniFile(*context, context->translator));
    EXPECT_EQ(context->settings.cutscenefile1, name);
    ASSERT_EQ(context->state.cutscenes, 1);
    EXPECT_EQ(context->state.csbrightness[0], 75);
    EXPECT_EQ(context->state.cslength[0], 3);
    EXPECT_EQ(context->state.cutscene[0][0], 'A');
    EXPECT_TRUE(std::filesystem::remove(scene));
}
TEST_F(CutsceneLoading, InvalidAdditionalMinutesRejectBeforePublishingSettings) {
    const auto settings = directory / "invalid.ini";
    {
        std::ofstream output(settings);
        output << "added_recording=" << std::numeric_limits<int>::max() << "\n";
    }
    context->state.ini_file.reset(myfopen(comskip::platform::path_to_utf8(settings).c_str(), "r"));
    ASSERT_TRUE(context->state.ini_file);
    const int previous_minutes = context->settings.added_recording;
    const int previous_search = context->settings.giveUpOnLogoSearch;
    try {
        LoadIniFile(*context, context->translator);
        FAIL() << "Unrepresentable recording duration was accepted";
    } catch (const comskip::ExitRequested& request) {
        EXPECT_EQ(request.status(), 1);
    }
    EXPECT_EQ(context->settings.added_recording, previous_minutes);
    EXPECT_EQ(context->settings.giveUpOnLogoSearch, previous_search);
}
