// The Combiner's contract table (docs/fingerprint-plan.md, "PART: Combiner").
#include "fingerprint/combiner.h"
#include "fingerprint/inputs.h"
#include "fingerprint/json.h"
#include <gtest/gtest.h>

namespace {
using namespace comskip::fingerprint;

Evidence evidence(std::vector<double> seams = {}, std::vector<Interval> logo = {}, std::vector<Block> blocks = {}) {
    Evidence result;
    result.capture_id = "c";
    result.revision = 3;
    result.final = true;
    result.logo_known = true;
    for (const double at : seams) result.seams.push_back({at, {"black"}});
    result.logo = std::move(logo);
    result.blocks = std::move(blocks);
    return result;
}
CombinerMatch ad(double from, double to) { return agreed_match(from, to, Status::ad); }
Mark mark(double start, double end, Basis basis, MarkKind kind = MarkKind::ad, MarkState state = MarkState::settled) {
    return {start, end, kind, state, basis};
}
const CombineMode final_mode{};
CombineMode live(double now) { return {false, now}; }

TEST(Combiner, JoinsMatchesInOneBreakAndMovesEdgesToSeams) {
    const auto copy = evidence({859.6});
    const std::vector matches{ad(860, 890), ad(920, 950)};
    EXPECT_EQ(combine(matches, &copy, final_mode), std::vector{mark(859.6, 950, Basis::fingerprint)});
}

TEST(Combiner, MatchesFarApartAreTwoBreaks) {
    const auto copy = evidence();
    const std::vector matches{ad(860, 890), ad(1000, 1030)};
    EXPECT_EQ(combine(matches, &copy, final_mode),
              (std::vector{mark(860, 890, Basis::fingerprint), mark(1000, 1030, Basis::fingerprint)}));
}

TEST(Combiner, ABreakExtendsOverTouchingCommercialBlocks) {
    const auto copy = evidence({}, {}, {{830, 860, true}, {860, 1200, false}});
    const std::vector matches{ad(860, 890)};
    EXPECT_EQ(combine(matches, &copy, final_mode), std::vector{mark(830, 890, Basis::both)});
}

TEST(Combiner, TheLogoClosesABreak) {
    const auto copy = evidence({}, {{892, 952}});
    const std::vector matches{ad(860, 890), ad(930, 950)};
    EXPECT_EQ(combine(matches, &copy, final_mode),
              (std::vector{mark(860, 890, Basis::fingerprint), mark(930, 950, Basis::fingerprint)}));
    // A seam within 1 s moves the close to it.
    const auto seam = evidence({890.8}, {{892, 952}});
    EXPECT_EQ(combine(std::vector{ad(860, 890)}, &seam, final_mode), std::vector{mark(860, 890.8, Basis::fingerprint)});
}

TEST(Combiner, AProgrammeBlockBetweenMatchesClosesTheBreak) {
    const auto copy = evidence({}, {}, {{0, 860, false}, {860, 895, true}, {895, 1500, false}});
    const std::vector matches{ad(860, 890), ad(930, 950)};
    EXPECT_EQ(combine(matches, &copy, final_mode),
              (std::vector{mark(860, 895, Basis::both), mark(930, 950, Basis::fingerprint)}));
}

TEST(Combiner, WithoutALogoOnlyShortGapsJoin) {
    auto copy = evidence();
    copy.logo_known = false;
    EXPECT_EQ(combine(std::vector{ad(860, 890), ad(915, 950)}, &copy, final_mode), std::vector{mark(860, 950, Basis::fingerprint)});
    EXPECT_EQ(combine(std::vector{ad(860, 890), ad(940, 950)}, &copy, final_mode),
              (std::vector{mark(860, 890, Basis::fingerprint), mark(940, 950, Basis::fingerprint)}));
}

TEST(Combiner, NoMatchesGiveComskipsBlocks) {
    const auto copy = evidence({}, {}, {{0, 99.2, false}, {99.267, 119.6, true}, {119.633, 400.8, true}, {400.833, 900, false},
                                        {900, 960, true}});
    EXPECT_EQ(combine({}, &copy, final_mode), (std::vector{mark(99.267, 400.8, Basis::comskip), mark(900, 960, Basis::comskip)}));
}

TEST(Combiner, AnInProgrammeAdIsMarkedOnItsOwn) {
    const auto copy = evidence({}, {{1400, 1478}, {1509, 1600}});
    const std::vector matches{ad(1300, 1330), agreed_match(1478, 1509, Status::in_programme_ad)};
    EXPECT_EQ(combine(matches, &copy, final_mode),
              (std::vector{mark(1300, 1330, Basis::fingerprint), mark(1478, 1509, Basis::fingerprint, MarkKind::in_programme_ad)}));
    // Inside an ad mark it is dropped.
    const auto block = evidence({}, {}, {{1470, 1520, true}});
    EXPECT_EQ(combine(std::vector{agreed_match(1478, 1509, Status::in_programme_ad)}, &block, final_mode),
              std::vector{mark(1470, 1520, Basis::comskip)});
}

TEST(Combiner, AnAdsAudioOverProgrammeIsAnInProgrammeAd) {
    // g4's Google split-screen: the logo stops 5.5 s before the ad's audio and
    // returns as it ends; only its start has a seam.
    const auto copy = evidence({4231.683}, {{4180, 4225.967}, {4261.967, 4291}});
    EXPECT_EQ(combine(std::vector{ad(4231.5, 4261.6)}, &copy, final_mode),
              std::vector{mark(4231.683, 4261.6, Basis::fingerprint, MarkKind::in_programme_ad)});
    // A break of one spot, bounded by seams at both ends, stays an ad.
    const auto spot = evidence({4231.6, 4261.7}, {{4180, 4225.967}, {4261.967, 4291}});
    EXPECT_EQ(combine(std::vector{ad(4231.5, 4261.6)}, &spot, final_mode), std::vector{mark(4231.6, 4261.7, Basis::fingerprint)});
}

TEST(Combiner, LiveMarksSettleWhenAgreedAndAired) {
    const auto copy = evidence();
    EXPECT_EQ(combine(std::vector{ad(860, 890)}, &copy, live(900)), std::vector{mark(860, 890, Basis::fingerprint)});
    const std::vector projected{CombinerMatch{600, 630, 600, 604, Status::ad}};
    EXPECT_EQ(combine(projected, &copy, live(605)),
              std::vector{mark(600, 630, Basis::fingerprint, MarkKind::ad, MarkState::provisional)});
    const std::vector joined{ad(860, 890), ad(920, 950)};
    EXPECT_EQ(combine(joined, &copy, live(960)),
              std::vector{mark(860, 950, Basis::fingerprint, MarkKind::ad, MarkState::provisional)});
    // Three minutes after its end it settles.
    EXPECT_EQ(combine(joined, &copy, live(1130)), std::vector{mark(860, 950, Basis::fingerprint)});
}

TEST(Combiner, IntroAndCreditsAreMarkedOverTheirAgreedPartAndGiveWayToAds) {
    const auto copy = evidence();
    const std::vector matches{agreed_match(120, 160, Status::intro), agreed_match(3400, 3460, Status::credits), ad(3430, 3445)};
    EXPECT_EQ(combine(matches, &copy, final_mode),
              (std::vector{mark(120, 160, Basis::fingerprint, MarkKind::intro), mark(3400, 3430, Basis::fingerprint, MarkKind::credits),
                           mark(3430, 3445, Basis::fingerprint), mark(3445, 3460, Basis::fingerprint, MarkKind::credits)}));
}

TEST(Combiner, NoEvidenceIsEvidenceMissing) {
    EXPECT_THROW(combine(std::vector{ad(860, 890)}, nullptr, final_mode), EvidenceMissing);
}

TEST(Combiner, WritesTheMarksFile) {
    const std::vector marks{mark(859.6, 950, Basis::both), mark(1478, 1509, Basis::fingerprint, MarkKind::in_programme_ad)};
    EXPECT_EQ(marks_file_json("cap-1", 12, true, marks),
              R"({"captureId":"cap-1","evidenceRevision":12,"final":true,"marks":[)"
              R"({"start":859.6,"end":950.0,"kind":"ad","state":"settled","basis":"both"},)"
              R"({"start":1478.0,"end":1509.0,"kind":"in-programme ad","state":"settled","basis":"fingerprint"}]})" "\n");
    EXPECT_EQ(marks_file_json("c", 0, false, {}), R"({"captureId":"c","evidenceRevision":0,"final":false,"marks":[]})" "\n");
    // Times to three decimals.
    const std::vector rounded{mark(859.60049, 950.0006, Basis::fingerprint)};
    EXPECT_NE(marks_file_json("c", 1, false, rounded).find(R"("start":859.6,"end":950.001,)"), std::string::npos);
}

// Names the app passes through: quotes, backslashes, control and non-ASCII
// text come back exactly from every document the tool writes or reads.
TEST(Inputs, RoundTripsNamesWithQuotesBackslashesAndNonAscii) {
    const std::string awkward = "plex:/Shows/\"Elsbeth\" \\ S03\\E03 caf\xc3\xa9 \xe2\x80\x94 \xf0\x9f\x93\xba\ttab";
    Context context;
    context.channel = "K\"B\\C \xc3\xa9";
    context.air_start = 1760832000.25;
    context.programmes.push_back({0, 3600, awkward, "series:\"x\"\\y"});
    const auto back = context_from_json(json::parse(context_to_json(context)));
    EXPECT_EQ(back.channel, context.channel);
    EXPECT_EQ(back.air_start, 1760832000.25);
    ASSERT_EQ(back.programmes.size(), 1u);
    EXPECT_EQ(back.programmes[0].title, awkward);
    EXPECT_EQ(back.programmes[0].series_key, "series:\"x\"\\y");

    Evidence evidence;
    evidence.capture_id = awkward;
    evidence.revision = 4;
    evidence.seams.push_back({12.3456, {"black"}});
    EXPECT_EQ(evidence_from_json(json::parse(evidence_to_json(evidence))).capture_id, awkward);
    EXPECT_EQ(evidence_from_json(json::parse(evidence_to_json(evidence))).seams[0].at, 12.346);

    const auto marks = json::parse(marks_file_json(awkward, 4, true, {}));
    EXPECT_EQ(marks.at("captureId").get<std::string>(), awkward);
}

TEST(Inputs, ReadsEvidenceAndContext) {
    const auto copy = evidence_from_json(json::parse(R"({ "captureId": "c9", "revision": 12, "final": false, "logoKnown": true,
        "seams": [ { "at": 859.6, "kinds": ["black", "silence"] } ], "logo": [ { "from": 0.0, "to": 118.2 } ],
        "blocks": [ { "from": 118.2, "to": 400.1, "score": 1.8, "decision": "commercial" },
                    { "from": 400.1, "to": 500, "score": null, "decision": "programme" } ] })"));
    EXPECT_EQ(copy.capture_id, "c9");
    EXPECT_EQ(copy.revision, 12);
    ASSERT_EQ(copy.seams.size(), 1u);
    EXPECT_EQ(copy.seams[0].kinds, (std::vector<std::string>{"black", "silence"}));
    ASSERT_EQ(copy.blocks.size(), 2u);
    EXPECT_TRUE(copy.blocks[0].commercial);
    EXPECT_FALSE(copy.blocks[1].commercial);
    EXPECT_EQ(evidence_from_json(json::parse(evidence_to_json(copy))).blocks.size(), 2u);
    EXPECT_THROW(evidence_from_json(json::parse(R"({ "revision": 1, "final": true, "logoKnown": true, "seams": [],
        "logo": [], "blocks": [ { "from": 1, "to": 2, "decision": "maybe" } ] })")), json::Invalid);
    EXPECT_THROW(json::parse("{ \"a\": 1 } x"), json::Invalid);

    const auto context = context_from_json(json::parse(R"({ "channel": null, "airStart": 1760832000, "source": "plex",
        "programmes": [ { "from": 0, "to": 3600, "title": "Elsbeth é", "seriesKey": "elsbeth" } ] })"));
    EXPECT_TRUE(context.channel.empty());
    EXPECT_EQ(context.source, "plex");
    ASSERT_EQ(context.programmes.size(), 1u);
    EXPECT_EQ(context.programmes[0].title, "Elsbeth \xc3\xa9");
    EXPECT_EQ(programme_at(context, 3599.9), &context.programmes[0]);
    EXPECT_EQ(programme_at(context, 3600), nullptr);
}
}
