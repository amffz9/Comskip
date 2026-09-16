#include "output/legacy_commands.h"
#include "output/legacy_edit_lists.h"
#include "output/plain_chapters.h"
#include <array>
#include <gtest/gtest.h>
#include <sstream>
using namespace comskip::output;
TEST(LegacyCutlistExports, WomblePreservesClipTypesNumbersAndSource) {
  std::ostringstream out;
  const std::array clips{WombleClip{1, true, 11, 10},
                         WombleClip{3, false, 42, 9}};
  write_womble(out, "Café.ts", clips);
  EXPECT_EQ(out.str(), "CLIPLIST: #1 commercial\nCLIP: Café.ts\n6 11 "
                       "10\nCLIPLIST: #3 show\nCLIP: Café.ts\n6 42 9\n");
}
TEST(LegacyCutlistExports, MlsPreservesHeaderCountAndPaddedFrames) {
  std::ostringstream out;
  const std::array marks{MlsBookmark{11, false}, MlsBookmark{21, true}};
  write_mls(out, "input.ts", 2, marks);
  EXPECT_EQ(out.str(),
            "[BookmarkList]\nPathName= input.ts\nVideoStreamID= 0\nFormat= "
            "frame\nCount= 2\n         11 0\n         21 1\n");
}
TEST(LegacyCutlistExports,
     CommandGoldenBytesPreserveOpenRangesPercentArgumentsAndTrailingSpace) {
  std::ostringstream mpgtx, dvr, schnitt;
  const std::array ranges{MpgtxRange{std::nullopt, CommandSeconds{61.9}},
                          MpgtxRange{CommandSeconds{120.9}, std::nullopt}};
  write_mpgtx(mpgtx, "header ", ranges);
  EXPECT_EQ(mpgtx.str(), "header [-0:01:01] [0:02:00-]\n");
  const std::array dvr_ranges{
      DvrCutRange{CommandSeconds{0}, CommandSeconds{61.9}}};
  write_dvrcut(dvr, "dvrcut \"%1\" \"%2\" ", dvr_ranges);
  EXPECT_EQ(dvr.str(), "dvrcut \"%1\" \"%2\" 0:00:00 0:01:01 \n");
  const std::array frame_ranges{Mpeg2SchnittRange{11, 21}};
  write_mpeg2schnitt(schnitt, "header ", frame_ranges);
  EXPECT_EQ(schnitt.str(), "header /o11 /i21 \n");
}
TEST(LegacyCutlistExports,
     PlainChapterHeaderAndRawFrameBoundariesRemainCompatible) {
  std::ostringstream out;
  const std::array<std::int64_t, 2> boundaries{75, 149};
  write_plain_chapters(out, {149, 2500}, boundaries);
  EXPECT_EQ(out.str(), "FILE PROCESSING COMPLETE    149 FRAMES AT  "
                       "2500\n-------------------\n75\n149\n");
}
TEST(LegacyCutlistExports, InvalidLateRecordsDoNotPartiallyWrite) {
  std::ostringstream out;
  const std::array clips{WombleClip{1, true, 1, 10},
                         WombleClip{0, false, 1, 10}};
  EXPECT_THROW(write_womble(out, "input.ts", clips), std::invalid_argument);
  const std::array ranges{Mpeg2SchnittRange{2, 1}};
  EXPECT_THROW(write_mpeg2schnitt(out, "header", ranges),
               std::invalid_argument);
  const std::array<std::int64_t, 1> negative{-1};
  EXPECT_THROW(write_plain_chapters(out, {149, 2500}, negative),
               std::invalid_argument);
  EXPECT_TRUE(out.str().empty());
}
TEST(LegacyCutlistExports, FailedStreamsAreReportedForEveryFormat) {
  std::ostringstream out;
  out.setstate(std::ios::badbit);
  EXPECT_THROW(write_womble(out, "", {}), std::runtime_error);
  EXPECT_THROW(write_mls(out, "", 0, {}), std::runtime_error);
  EXPECT_THROW(write_mpgtx(out, "", {}), std::runtime_error);
  EXPECT_THROW(write_dvrcut(out, "", {}), std::runtime_error);
  EXPECT_THROW(write_mpeg2schnitt(out, "", {}), std::runtime_error);
  EXPECT_THROW(write_plain_chapters(out, {1, 2500}, {}), std::runtime_error);
}
