#include "recording_context.h"
#include "exit_requested.h"
#include "checked_format.h"
#include "detection/legacy_detection.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>

namespace {
class DetectionWarnings : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-warning-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 1;
        comskip::checked_format(context->state.logfilename, "%s", (directory / "warnings.log").string().c_str());
    }
    void TearDown() override {
        context.reset();
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    std::string log() {
        std::ifstream input(directory / "warnings.log");
        return {std::istreambuf_iterator<char>(input), {}};
    }
};
TEST_F(DetectionWarnings, EmptyCutfileReportsItsFilenameInSpanishWithoutInvalidVarargs) {
    context->translator = comskip::localization::Translator("es");
    const auto filename = (directory / "empty.cut").string();
    { std::ofstream empty(filename, std::ios::binary); }
    EXPECT_NO_THROW(LoadCutScene(*context, filename.c_str()));
    EXPECT_EQ(context->state.cutscenes, 0);
    EXPECT_EQ(log(), "ERROR: No se pudo cargar el archivo de corte \"" + filename + "\"\n");
    EXPECT_FALSE(context->state.cutscene_file);
}
TEST_F(DetectionWarnings, MissingCutfileUsesEnglishFallback) {
    using comskip::config::Ini;
    context->translator = comskip::localization::Translator("es",
        Ini("detection_cutfile_open_failed=\"Can't open cutfile \\\"{}\\\"\\n\"\n"), Ini{});
    const auto filename = (directory / "missing.cut").string();
    LoadCutScene(*context, filename.c_str());
    EXPECT_EQ(log(), "Can't open cutfile \"" + filename + "\"\n");
}
TEST_F(DetectionWarnings, OptionalLogoSaveFailureReturnsWithoutWritingToNullFile) {
    context->translator = comskip::localization::Translator("es");
    context->settings.startOverAfterLogoInfoAvail = false;
    comskip::checked_format(context->state.logofilename, "%s", (directory / "missing-directory" / "logo.txt").string().c_str());
    EXPECT_NO_THROW(SaveLogoMaskData(*context));
    EXPECT_NE(log().find("no se pudo crear el archivo"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(directory / "missing-directory"));
}
TEST_F(DetectionWarnings, RequiredLogoSaveFailurePreservesExitStatusAndReleasesOwnership) {
    context->settings.startOverAfterLogoInfoAvail = true;
    comskip::checked_format(context->state.logofilename, "%s",
        (directory / "missing-directory" / "logo.txt").string().c_str());
    try {
        SaveLogoMaskData(*context);
        FAIL() << "Required logo save must report failure";
    } catch (const comskip::ExitRequested& error) {
        EXPECT_EQ(error.status(), 7);
    }
    EXPECT_NE(log().find("logo.txt"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(directory / "missing-directory"));
}
}
