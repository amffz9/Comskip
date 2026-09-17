#include "recording_context.h"
#include "logo_detection.h"
#include "scene_analysis.h"
#include "checked_format.h"
#include "localization/diagnostic_render.h"
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
}
TEST_F(DetectionWarnings, MissingCutfileUsesEnglishFallback) {
    using comskip::config::Ini;
    context->translator = comskip::localization::Translator("es",
        Ini("detection_cutfile_open_failed=\"Can't open cutfile \\\"{}\\\"\\n\"\n"), Ini{});
    const auto filename = (directory / "missing.cut").string();
    LoadCutScene(*context, filename.c_str());
    EXPECT_EQ(log(), "Can't open cutfile \"" + filename + "\"\n");
}
TEST_F(DetectionWarnings, CutsceneSaveOpenFailureReturnsOwnedDiagnostic) {
    context->settings.cutscenefile = (directory / "missing" / "scene.cut").string();
    context->state.width = context->state.videowidth = context->state.height = 2;
    unsigned char pixels[4]{};
    context->state.frame_ptr = pixels;
    try {
        RecordCutScene(*context, 1, 20);
        FAIL() << "Expected an output-open diagnostic";
    } catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code, comskip::diagnostics::Code::output_open);
        ASSERT_EQ(error.diagnostic().arguments.size(), 1u);
        EXPECT_EQ(error.diagnostic().arguments[0], context->settings.cutscenefile);
    }
}
TEST_F(DetectionWarnings, OptionalLogoSaveFailureReturnsAnOwnedDiagnostic) {
    context->translator = comskip::localization::Translator("es");
    context->settings.startOverAfterLogoInfoAvail = false;
    comskip::checked_format(context->state.logofilename, "%s", (directory / "missing-directory" / "logo.txt").string().c_str());
    try {
        SaveLogoMaskData(*context);
        FAIL() << "Logo save must report its open failure";
    } catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code, comskip::diagnostics::Code::output_open);
        ASSERT_EQ(error.diagnostic().arguments.size(), 1u);
        EXPECT_EQ(error.diagnostic().arguments.front(), context->state.logofilename);
        EXPECT_EQ(comskip::localization::render_diagnostic(error.diagnostic(), context->translator),
            "No se pudo abrir el archivo de salida: " + context->state.logofilename);
    }
    EXPECT_TRUE(log().empty());
    EXPECT_FALSE(std::filesystem::exists(directory / "missing-directory"));
}
TEST_F(DetectionWarnings, RequiredLogoSaveFailureUsesTheSameOwnedDiagnostic) {
    context->settings.startOverAfterLogoInfoAvail = true;
    comskip::checked_format(context->state.logofilename, "%s",
        (directory / "missing-directory" / "logo.txt").string().c_str());
    try {
        SaveLogoMaskData(*context);
        FAIL() << "Required logo save must report failure";
    } catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code, comskip::diagnostics::Code::output_open);
        ASSERT_EQ(error.diagnostic().arguments.size(), 1u);
        EXPECT_EQ(error.diagnostic().arguments.front(), context->state.logofilename);
        EXPECT_EQ(comskip::localization::render_diagnostic(error.diagnostic(), context->translator),
            "Could not open output file: " + context->state.logofilename);
    }
    EXPECT_TRUE(log().empty());
    EXPECT_FALSE(std::filesystem::exists(directory / "missing-directory"));
}
}
