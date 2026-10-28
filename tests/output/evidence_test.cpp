#include "output/evidence.h"
#include "detection/frame_causes.h"
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <vector>

namespace {
using comskip::detection::FrameCause;
using comskip::detection::cause_value;
using namespace comskip::output;

const FrameTime at_25_fps = [](long frame) { return frame / 25.0; };

black_frame_info cut(long frame, int cause) { return {frame, 0, 0, 0, cause}; }

TEST(Evidence, WritesTheCrossRepoFieldsInProxySeconds) {
    Evidence evidence;
    evidence.revision = 12;
    evidence.logo_known = true;
    evidence.seams = {{859.6, static_cast<unsigned>(SeamKind::black) | static_cast<unsigned>(SeamKind::silence)}};
    evidence.logo = {{0.0, 118.2}};
    evidence.blocks = {{118.2, 400.1, 1.8, BlockDecision::commercial}, {400.1, 900.0, std::nullopt, BlockDecision::programme}};
    std::ostringstream output;
    write_evidence(output, evidence);
    EXPECT_EQ(output.str(),
              R"({"revision":12,"final":false,"logoKnown":true,"seams":[{"at":859.6,"kinds":["black","silence"]}],)"
              R"("logo":[{"from":0.0,"to":118.2}],"blocks":[{"from":118.2,"to":400.1,"score":1.8,"decision":"commercial"},)"
              R"({"from":400.1,"to":900.0,"score":null,"decision":"programme"}]})" "\n");
    // Times are rounded to three decimals and scores to two.
    evidence.seams = {{859.60049, 1}};
    evidence.blocks = {{118.2004, 400.1006, 6.125, BlockDecision::commercial}};  // a tie rounds to even, as before
    std::ostringstream rounded;
    write_evidence(rounded, evidence);
    EXPECT_NE(rounded.str().find(R"("at":859.6,)"), std::string::npos) << rounded.str();
    EXPECT_NE(rounded.str().find(R"({"from":118.2,"to":400.101,"score":6.12)"), std::string::npos) << rounded.str();
}

TEST(Evidence, WritesEmptyListsAndRejectsNonFiniteNumbers) {
    Evidence evidence;
    evidence.revision = 1;
    evidence.final = true;
    std::ostringstream output;
    write_evidence(output, evidence);
    EXPECT_EQ(output.str(), R"({"revision":1,"final":true,"logoKnown":false,"seams":[],"logo":[],"blocks":[]})" "\n");
    evidence.logo = {{0.0, std::numeric_limits<double>::infinity()}};
    std::ostringstream rejected;
    EXPECT_THROW(write_evidence(rejected, evidence), std::invalid_argument);
    EXPECT_TRUE(rejected.str().empty());
}

TEST(Evidence, GroupsCutFramesLessThanASecondApartIntoOneSeam) {
    const std::vector<black_frame_info> cuts{
        cut(100, cause_value(FrameCause::black)), cut(101, cause_value(FrameCause::black)),
        cut(120, cause_value(FrameCause::silence)),
        cut(500, cause_value(FrameCause::scene_change)),
        cut(700, cause_value(FrameCause::non_uniform) | cause_value(FrameCause::aspect_ratio)),
        cut(900, cause_value(FrameCause::cutscene)),
        cut(1000, cause_value(FrameCause::black))};
    const auto seams = evidence_seams(cuts, 1000, at_25_fps);
    ASSERT_EQ(seams.size(), 3u);
    EXPECT_DOUBLE_EQ(seams[0].at, (4.0 + 4.8) / 2);
    EXPECT_EQ(seams[0].kinds, static_cast<unsigned>(SeamKind::black) | static_cast<unsigned>(SeamKind::silence));
    EXPECT_DOUBLE_EQ(seams[1].at, 20.0);
    EXPECT_EQ(seams[1].kinds, static_cast<unsigned>(SeamKind::scene));
    EXPECT_DOUBLE_EQ(seams[2].at, 28.0);
    EXPECT_EQ(seams[2].kinds, static_cast<unsigned>(SeamKind::black) | static_cast<unsigned>(SeamKind::aspect));
}

TEST(Evidence, LogoRunsEndAtTheFirstFrameWithoutALogo) {
    std::vector<frame_info> frames(10);
    for (std::size_t i : {2u, 3u, 4u, 8u, 9u}) frames[i].logo_present = true;
    const auto logo = evidence_logo(frames, 1, at_25_fps);
    ASSERT_EQ(logo.size(), 2u);
    EXPECT_DOUBLE_EQ(logo[0].from, 3 / 25.0);
    EXPECT_DOUBLE_EQ(logo[0].to, 6 / 25.0);
    EXPECT_DOUBLE_EQ(logo[1].from, 9 / 25.0);
    EXPECT_DOUBLE_EQ(logo[1].to, 10 / 25.0);
}
}
