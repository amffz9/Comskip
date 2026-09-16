#include "output/legacy_cutlist_adapter.h"
#include "platform/utf8_paths.h"
#include "recording_context.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <random>
namespace {
class LegacyCutlistAdapter : public ::testing::Test {
protected:
  std::filesystem::path directory;
  std::unique_ptr<RecordingContext> context;
  void SetUp() override {
    directory =
        std::filesystem::temp_directory_path() /
        ("comskip-final-cutlist-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(std::random_device{}()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    context = std::make_unique<RecordingContext>();
    auto &s = context->state;
    auto &o = context->settings;
    s.outbasename = comskip::platform::path_to_utf8(directory / "result");
    s.inbasename = s.outbasename;
    s.mpegfilename = s.outbasename + ".ts";
    s.frame_count = 50;
    s.framenum_real = 50;
    o.fps = 25;
    o.output_womble = true;
    o.output_mls = true;
    o.output_mpgtx = true;
    o.output_dvrcut = true;
    o.output_mpeg2schnitt = true;
    o.output_chapters = true;
  }
  void TearDown() override {
    context.reset();
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
  }
  std::string read(const char *suffix) {
    std::ifstream in(directory / (std::string("result") + suffix));
    return {std::istreambuf_iterator<char>(in), {}};
  }
};
TEST_F(LegacyCutlistAdapter,
       ActualReviewMarksPreserveNumberingFiltersAndTerminalTail) {
  context->state.reffer = {{10, 20}, {30, 40}};
  context->state.reffer_count = 1;
  WriteLegacyCutlistFiles(*context, true);
  EXPECT_NE(read(".wme").find("CLIPLIST: #3 show\n"), std::string::npos);
  EXPECT_NE(read(".mls").find("Count= 4\n         11 0\n         21 1\n        "
                              " 31 0\n         41 1\n"),
            std::string::npos);
  EXPECT_NE(read("_mpgtx.bat").find("[-0:00:00] [-0:00:01] [0:00:01-]\n"),
            std::string::npos);
  EXPECT_EQ(read("_dvrcut.bat"), "dvrcut \"%1\" \"%2\" \n");
  EXPECT_EQ(read("_mpeg2schnitt.bat"), "mpeg2schnitt.exe /S /E /R25.00  /Z "
                                       "\"%2\" \"%1\" /o11 /i21 /o31 /i41 \n");
}
TEST_F(LegacyCutlistAdapter,
       EmptyNormalListHasCompleteHeadersAndLegacyTerminalCommand) {
  context->state.commercial_count = -1;
  WriteLegacyCutlistFiles(*context);
  EXPECT_EQ(
      read(".chap"),
      "FILE PROCESSING COMPLETE     49 FRAMES AT  2500\n-------------------\n");
  EXPECT_NE(read("_mpgtx.bat").find("[0:00:00-]\n"), std::string::npos);
  EXPECT_EQ(read("_dvrcut.bat"), "dvrcut \"%1\" \"%2\" 0:00:00 0:00:01 \n");
}
TEST_F(LegacyCutlistAdapter,
       ExpandedDvrHeaderOptionsAndPlainRawBoundariesAreExact) {
  context->state.commercial_count = -1;
  context->settings.dvrcut_options = "%s|%s|%s|%% ";
  context->state.block_count = 2;
  context->state.cblock.resize(3, comskip::detection::empty_block());
  context->state.cblock[0].f_end = 20;
  context->state.cblock[1].f_end = 49;
  WriteLegacyCutlistFiles(*context);
  EXPECT_EQ(read("_dvrcut.bat"),
            context->state.inbasename + "|" + context->state.inbasename + "|" +
                context->state.inbasename + "|% 0:00:00 0:00:01 \n");
  EXPECT_EQ(read(".chap"), "FILE PROCESSING COMPLETE     49 FRAMES AT  "
                           "2500\n-------------------\n20\n49\n");
}
TEST_F(LegacyCutlistAdapter, OutputSwitchesDoNotChangeSelectedScriptRecords) {
  context->state.reffer = {{10, 20}, {30, 40}};
  context->state.reffer_count = 1;
  WriteLegacyCutlistFiles(*context, true);
  const auto expected = read("_mpgtx.bat");
  context->settings.output_womble = false;
  context->settings.output_mls = false;
  context->settings.output_dvrcut = false;
  context->settings.output_mpeg2schnitt = false;
  context->settings.output_chapters = false;
  WriteLegacyCutlistFiles(*context, true);
  EXPECT_EQ(read("_mpgtx.bat"), expected);
}
} // namespace
