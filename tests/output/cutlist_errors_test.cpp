#include "recording_context.h"
#include "output/frame_script_adapter.h"
#include "output/player_export_adapter.h"
#include "output/legacy_cutlist_adapter.h"
#include "detection/legacy_detection.h"
#include "output/cutlist_exports.h"
#include "checked_format.h"
#include "exit_requested.h"
#include "diagnostic_render.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>

namespace {
class CutlistErrors : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-cuterror-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->state.output_console = false;
        context->settings.verbose = 0;
        comskip::checked_format(context->state.logfilename, "%s", (directory / "error.log").string().c_str());
        comskip::checked_format(context->state.outbasename, "%s", (directory / "missing" / "result").string().c_str());
        comskip::checked_format(context->state.out_filename, "%s", (directory / "missing" / "result.txt").string().c_str());
    }
    void TearDown() override {
        context.reset();
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    void expect_exit(int status) {
        try {
            OpenOutputFiles(*context);
            context->state.frame_count=50;
            context->state.framenum_real=50;
            context->state.commercial_count=-1;
            WritePlayerExportFiles(*context);
            WriteLegacyCutlistFiles(*context);
            FAIL() << "Expected output creation failure";
        }
        catch (const comskip::ExitRequested& exit) { EXPECT_EQ(exit.status(), status); }
    }
    std::string expect_output_open() {
        try {
            OpenOutputFiles(*context);
            context->state.frame_count=50;
            context->state.framenum_real=50;
            context->state.commercial_count=-1;
            WritePlayerExportFiles(*context);
            WriteLegacyCutlistFiles(*context);
            ADD_FAILURE() << "Expected output-open diagnostic";
        }
        catch (const comskip::diagnostics::DiagnosticProvider& error) {
            EXPECT_EQ(error.diagnostic().code, comskip::diagnostics::Code::output_open);
            return comskip::localization::render_diagnostic(error.diagnostic(), context->translator);
        }
        return {};
    }
    std::string log() {
        std::ifstream input(directory / "error.log");
        return {std::istreambuf_iterator<char>(input), {}};
    }
};
TEST_F(CutlistErrors, DefaultOutputRetryReportsFilenameInEnglish) {
    context->settings.output_default = true;
    expect_exit(103);
    EXPECT_EQ(log(), "ERROR writing to " + std::string(context->state.out_filename) + "\n");
    EXPECT_FALSE(context->state.out_file);
}
TEST_F(CutlistErrors, ChapterOutputRetryReportsSpanishAndReleasesOwner) {
    context->translator = comskip::localization::Translator("es");
    context->settings.output_default = false;
    context->settings.output_chapters = true;
    EXPECT_EQ(expect_output_open(), "No se pudo abrir el archivo de salida: " +
                                      std::string(context->state.outbasename) + ".chap");
    EXPECT_FALSE(std::filesystem::exists(context->state.outbasename + ".chap"));
}
TEST_F(CutlistErrors, ZoomPlayerCreationFailurePreservesExitAndLocalizesStderr) {
    context->translator = comskip::localization::Translator("es");
    context->settings.output_default = false;
    context->settings.output_chapters = false;
    context->settings.output_zoomplayer_cutlist = true;
    EXPECT_EQ(expect_output_open(), "No se pudo abrir el archivo de salida: " +
                                      std::string(context->state.outbasename) + ".cut");
    EXPECT_FALSE(std::filesystem::exists(context->state.outbasename + ".cut"));
}
TEST_F(CutlistErrors, ValidatedOutputTemplatesExpandStringsAndEscapedPercentExactly) {
    context->settings = comskip::config::load_settings(comskip::config::Ini(
        "fps=25\noutput_default=0\noutput_avisynth=1\noutput_dvrcut=1\n"
        "avisynth_options=\"%% %s\"\ndvrcut_options=\"%s|%s|%s|%%\""));
    context->state.mpegfilename = (directory / "input.ts").string();
    comskip::checked_format(context->state.outbasename, "%s", (directory / "result").string().c_str());
    comskip::checked_format(context->state.inbasename, "%s", "input");
    OpenOutputFiles(*context);
    context->state.frame_count=50;
    context->state.framenum_real=50;
    context->state.commercial_count=-1;
    WriteFrameScriptFiles(*context);
    WriteLegacyCutlistFiles(*context);
    const auto read = [](const std::filesystem::path& path) {
        std::ifstream file(path);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    EXPECT_EQ(read(directory / "input.ts.avs"), "% " + context->state.mpegfilename + "trim(1,49)\n");
    EXPECT_EQ(read(directory / "result_dvrcut.bat"), "input|input|input|%0:00:00 0:00:01 \n");
}
}
