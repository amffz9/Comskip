// LiveMatcher's contract table (docs/fingerprint-plan.md, "PART: LiveMatcher") on
// synthetic codes, the live job's input queue, and the Combiner's live settling.
#include "fingerprint/audio_queue.h"
#include "fingerprint/combiner.h"
#include "fingerprint/live_matcher.h"
#include "fingerprint/logfmt.h"
#include "fingerprint/process_usage.h"
#include "fingerprint/matching.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <random>

namespace {
using namespace comskip::fingerprint;
using namespace std::chrono_literals;

std::vector<std::uint32_t> noise(unsigned seed, long count) {
    std::mt19937 random(seed);
    std::vector<std::uint32_t> codes(static_cast<std::size_t>(count));
    for (auto& code : codes) code = random();
    return codes;
}
long code_at(double seconds) { return std::lround(seconds / code_period); }
// The capture second reached once code k is complete.
double reached(long k) { return (static_cast<double>(k) * code_samples + code_span_samples) / sample_rate; }

std::shared_ptr<const LiveIndex> index_of(std::initializer_list<std::pair<long long, const std::vector<std::uint32_t>*>> entries) {
    auto index = std::make_shared<LiveIndex>();
    for (const auto& [id, codes] : entries) index->add(id, Status::ad, *codes);
    index->finish();
    return index;
}

// A recording of noise; feed() hands its codes to the matcher up to a time.
struct Replay {
    std::vector<std::uint32_t> codes;
    LiveMatcher matcher;
    long fed = 0;
    explicit Replay(double seconds, unsigned seed = 1, MatchRules rules = {})
        : codes(noise(seed, code_at(seconds))), matcher(rules) {
        matcher.run_start(0);
    }
    Replay& put(double at, std::span<const std::uint32_t> clip) {
        std::ranges::copy(clip, codes.begin() + code_at(at));
        return *this;
    }
    // Feeds every code whose audio is complete by capture second `until`.
    void feed(double until) {
        long last = fed;
        while (last < static_cast<long>(codes.size()) && reached(last) <= until) ++last;
        matcher.feed(std::span(codes).subspan(static_cast<std::size_t>(fed), static_cast<std::size_t>(last - fed)));
        fed = last;
    }
    const LiveMatch* only() const {
        const auto& matches = matcher.matches();
        return matches.size() == 1 ? &matches[0] : nullptr;
    }
};

TEST(LiveMatcher, ReportsAMatchAfterThreeSecondsAndProjectsTheRest) {
    const auto ad = noise(7, code_at(30));
    Replay replay(700);
    replay.put(600, ad);
    replay.matcher.refresh(index_of({{11, &ad}}));
    replay.feed(600 + 2.72 + 2.0);
    EXPECT_TRUE(replay.matcher.matches().empty());
    replay.feed(600 + 2.72 + 4.0);
    const auto* match = replay.only();
    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->entry, 11);
    EXPECT_EQ(match->status, Status::ad);
    EXPECT_NEAR(match->agreed_from, 600, 0.8);
    EXPECT_NEAR(match->from, match->agreed_from, 1e-9);
    EXPECT_GE(match->agreed_to - match->agreed_from, 3.0);
    EXPECT_LE(match->agreed_to, 605);
    EXPECT_NEAR(match->to, 630, 1.0);
    EXPECT_TRUE(match->projected());
    EXPECT_FALSE(match->ended);

    // The codes agree to the end: agreed over the whole extent, nothing projected.
    replay.feed(640);
    match = replay.only();
    ASSERT_NE(match, nullptr);
    EXPECT_TRUE(match->ended);
    EXPECT_FALSE(match->withdrawn);
    EXPECT_FALSE(match->projected());
    EXPECT_NEAR(match->agreed_to, 630, 1.0);
}

TEST(LiveMatcher, ADivergenceWithdrawsTheProjectedPartWithinTwoSeconds) {
    // A 15 s cut of a 30 s ad: the codes stop agreeing at 615.
    const auto ad = noise(7, code_at(30));
    Replay replay(700);
    replay.put(600, std::span(ad).first(static_cast<std::size_t>(code_at(15))));
    replay.matcher.refresh(index_of({{11, &ad}}));
    replay.feed(615.5);
    ASSERT_NE(replay.only(), nullptr);
    EXPECT_FALSE(replay.only()->withdrawn);
    // The newest code to agree ends 2.72 s after it starts; the first that
    // does not is complete by about 617.
    double stopped = -1;
    double at = 615.5;
    for (; at < 625 && !replay.only()->withdrawn; at += 0.1) {
        replay.feed(at);
        if (stopped < 0 && replay.only()->stopped_at >= 0) stopped = replay.only()->stopped_at;
    }
    const auto* match = replay.only();
    ASSERT_TRUE(match->withdrawn);
    ASSERT_GE(stopped, 0);
    EXPECT_LE(match->ended_at - stopped, 2.0);
    EXPECT_GT(match->ended_at - stopped, 1.0);
    EXPECT_NEAR(match->to, 615, 1.0);
    EXPECT_EQ(match->to, match->agreed_to);
    EXPECT_FALSE(match->projected());
}

TEST(LiveMatcher, ShortAgreementIsNoMatch) {
    const auto ad = noise(7, code_at(30));
    Replay replay(700);
    replay.put(600, std::span(ad).first(static_cast<std::size_t>(code_at(2))));
    replay.matcher.refresh(index_of({{11, &ad}}));
    replay.feed(700);
    EXPECT_TRUE(replay.matcher.matches().empty());
}

TEST(LiveMatcher, ARefreshedIndexMatchesFromThenOn) {
    const auto ad = noise(7, code_at(30));
    Replay replay(700);
    replay.put(100, ad).put(500, ad);
    replay.matcher.refresh(index_of({}));
    replay.feed(300);
    EXPECT_TRUE(replay.matcher.matches().empty());
    replay.matcher.refresh(index_of({{12, &ad}}));
    replay.feed(700);
    // The second airing; the first was before the entry was known, and it is
    // more than 20 s before the second so it is a self-repeat too.
    const auto& matches = replay.matcher.matches();
    const auto found = std::ranges::find(matches, 12LL, &LiveMatch::entry);
    ASSERT_NE(found, matches.end());
    EXPECT_NEAR(found->agreed_from, 500, 0.8);
    EXPECT_EQ(std::ranges::count(matches, 12LL, &LiveMatch::entry), 1);
}

TEST(LiveMatcher, NothingIsJoinedAcrossARunBoundary) {
    const auto ad = noise(7, code_at(30));
    LiveMatcher matcher;
    matcher.refresh(index_of({{11, &ad}}));
    auto first = noise(1, code_at(600));
    first.insert(first.end(), ad.begin(), ad.begin() + code_at(10));
    matcher.run_start(0);
    matcher.feed(first);
    // The second run begins with the rest of the ad.
    std::vector<std::uint32_t> second(ad.begin() + code_at(10), ad.end());
    const auto more = noise(2, code_at(60));
    second.insert(second.end(), more.begin(), more.end());
    matcher.run_start(620);
    matcher.feed(second);
    matcher.finish();
    const auto& matches = matcher.matches();
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_TRUE(matches[0].withdrawn);
    EXPECT_LE(matches[0].to, 611);
    EXPECT_GE(matches[1].from, 620);
    EXPECT_NEAR(matches[1].agreed_to, 640, 1.0);
}

TEST(LiveMatcher, FindsARepeatOfTheRecordingsOwnCodes) {
    const auto spot = noise(9, code_at(30));
    Replay replay(400);
    replay.put(100, spot).put(250, spot);
    replay.feed(400);
    replay.matcher.finish();
    const auto& matches = replay.matcher.matches();
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_TRUE(matches[0].self());
    EXPECT_NEAR(matches[0].agreed_from, 250, 0.8);
    EXPECT_NEAR(matches[0].agreed_to, 280, 1.0);
    EXPECT_NEAR(matches[0].earlier_from, 100, 0.8);
    EXPECT_NEAR(matches[0].earlier_to, 130, 1.0);
    EXPECT_FALSE(matches[0].projected());
}

TEST(LiveMatcher, ACommonCodeIsNoSeed) {
    // A held tone: one value in more places than a seed may be.
    std::vector<std::uint32_t> held(40, 0x12345678u);
    LiveIndex index;
    index.add(1, Status::ad, held);
    index.finish();
    std::vector<SeedIndex::Place> hits;
    index.hits(0x12345678u, near_seed_bits, hits);
    EXPECT_TRUE(hits.empty());
    // A code near the held one is no seed either.
    index.hits(0x12345679u, near_seed_bits, hits);
    EXPECT_TRUE(hits.empty());
    const auto ad = noise(3, 50);
    LiveIndex other;
    other.add(4, Status::in_programme_ad, held);
    other.add(5, Status::ad, ad);
    other.finish();
    other.hits(ad[17] ^ 0x00010001u, near_seed_bits, hits);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(other.id(hits[0].sequence), 5);
    EXPECT_EQ(hits[0].position, 17);
    other.hits(ad[17] ^ 0x00010001u, 0, hits);
    EXPECT_TRUE(hits.empty());
    EXPECT_EQ(other.status(0), Status::in_programme_ad);
}

TEST(AudioQueue, DropsAndCountsWhatItCannotHoldAndRestartsTheRun) {
    AudioQueue queue(30 * sample_rate);
    const std::vector<std::int16_t> second(sample_rate, 1);
    queue.run_start(100);
    for (int index = 0; index < 40; ++index) queue.audio(second);  // nobody reads: never waits
    EXPECT_EQ(queue.dropped_samples(), 10 * sample_rate);
    EXPECT_EQ(queue.peak_samples(), 30u * sample_rate);
    std::size_t audio = 0;
    auto piece = queue.pop(0ms);
    ASSERT_TRUE(piece);
    EXPECT_EQ(piece->kind, AudioPiece::Kind::run);
    EXPECT_EQ(piece->start, 100);
    while ((piece = queue.pop(0ms))) audio += piece->samples.size();
    EXPECT_EQ(audio, 30u * sample_rate);
    // The next audio kept starts a new run where it belongs: 40 s after 100.
    queue.audio(second);
    queue.end();
    queue.close();
    piece = queue.pop(0ms);
    ASSERT_TRUE(piece);
    EXPECT_EQ(piece->kind, AudioPiece::Kind::run);
    EXPECT_DOUBLE_EQ(piece->start, 140);
    EXPECT_EQ(queue.pop(0ms)->kind, AudioPiece::Kind::audio);
    EXPECT_EQ(queue.pop(0ms)->kind, AudioPiece::Kind::end);
    EXPECT_FALSE(queue.pop(0ms));
    EXPECT_TRUE(queue.drained());
}

Mark ad_mark(double start, double end, MarkState state) { return {start, end, MarkKind::ad, state, Basis::fingerprint}; }

TEST(MarkHistory, AProvisionalMarkSettlesOnlyAfterThreeMinutesWithoutChange) {
    Evidence copy;
    copy.logo_known = true;
    const std::vector joined{agreed_match(860, 890, Status::ad), agreed_match(920, 950, Status::ad)};
    MarkHistory history;
    const auto live = [&](std::span<const CombinerMatch> matches, double now) {
        return history.settle(combine(matches, &copy, {false, now}), now);
    };
    // First seen at 960, so settled 3 minutes later, not 3 minutes after its end.
    EXPECT_EQ(live(joined, 960), std::vector{ad_mark(860, 950, MarkState::provisional)});
    EXPECT_EQ(live(joined, 1130), std::vector{ad_mark(860, 950, MarkState::provisional)});
    EXPECT_EQ(live(joined, 1140), std::vector{ad_mark(860, 950, MarkState::settled)});
    // It changes at 1200 (another match joins): provisional again until 1380.
    const std::vector grown{agreed_match(860, 890, Status::ad), agreed_match(920, 950, Status::ad), agreed_match(980, 1000, Status::ad)};
    EXPECT_EQ(live(grown, 1200), std::vector{ad_mark(860, 1000, MarkState::provisional)});
    EXPECT_EQ(live(grown, 1379), std::vector{ad_mark(860, 1000, MarkState::provisional)});
    EXPECT_EQ(live(grown, 1380), std::vector{ad_mark(860, 1000, MarkState::settled)});
    // A fully agreed mark that has aired settles at once.
    MarkHistory fresh;
    EXPECT_EQ(fresh.settle(combine(std::vector{agreed_match(860, 890, Status::ad)}, &copy, {false, 900}), 900),
              std::vector{ad_mark(860, 890, MarkState::settled)});
}

TEST(Combiner, ASelfRepeatIsAnAdOnlyUnderTheWithinShowRule) {
    Evidence copy;
    copy.logo_known = true;
    copy.seams = {{100, {"black"}}, {130, {"black"}}};
    copy.logo = {{0, 99}, {131, 249}, {281, 400}};
    EXPECT_TRUE(within_show_ad(copy, 100.3, 130.2, 250.4, 280.1));
    EXPECT_FALSE(within_show_ad(copy, 100.3, 121.2, 250.4, 271.1));  // 21 s: not a standard length
    copy.logo.push_back({255, 270});
    EXPECT_FALSE(within_show_ad(copy, 100.3, 130.2, 250.4, 280.1));  // logo during the second copy
    copy.logo.pop_back();
    copy.seams = {{100, {"black"}}};
    EXPECT_FALSE(within_show_ad(copy, 100.3, 130.2, 250.4, 280.1));  // no copy bounded at both ends
    copy.seams = {{100, {"black"}}, {130, {"black"}}};
    copy.logo_known = false;
    EXPECT_FALSE(within_show_ad(copy, 100.3, 130.2, 250.4, 280.1));  // no logo learned: can't show none
}

TEST(Logfmt, LinesStartWithEventWallNowAndQuoteOnlyWhatNeedsIt) {
    using namespace comskip::fingerprint::logfmt;
    EXPECT_EQ(line("revision", 12.3454, 1200, {integer("revision", 5)}), "event=revision wall=12.345 now=1200.000 revision=5\n");
    EXPECT_EQ(line("stall", 0, 1.5), "event=stall wall=0.000 now=1.500\n");
    EXPECT_EQ(line("mark", 1, 2, {seconds("start", 859.6), text("kind", "in-programme ad"), text("basis", "both"), flag("ended", true)}),
              "event=mark wall=1.000 now=2.000 start=859.600 kind=\"in-programme ad\" basis=both ended=1\n");
    EXPECT_EQ(text("id", "a=\"b\"\\c\n\x01").value, R"("a=\"b\"\\c\n\u0001")");
    EXPECT_EQ(text("id", "").value, R"("")");
    EXPECT_EQ(text("id", "caf\xc3\xa9").value, "caf\xc3\xa9");
}

TEST(ProcessUsage, ReportsMemoryAndCpu) {
    const auto usage = comskip::fingerprint::process_usage();
    EXPECT_GT(usage.rss_bytes, 0u);
    EXPECT_GE(usage.peak_rss_bytes, usage.rss_bytes);
    EXPECT_GE(usage.cpu_seconds, 0.0);
}

// OwnPlaces gives what the map it replaced gave: each code's places in the
// order added, until a code is held in seed_max_occurrences places.
TEST(OwnPlaces, MatchesAMapOfPlacesAcrossMerges) {
    using comskip::fingerprint::OwnPlaces;
    using comskip::fingerprint::seed_max_occurrences;
    OwnPlaces own(0);
    std::map<std::uint32_t, std::vector<std::uint32_t>> reference;
    std::map<std::uint32_t, bool> saturated;
    std::mt19937 random(7);
    std::vector<std::uint32_t> found;
    for (std::uint32_t place = 0; place < 30000; ++place) {
        // Mostly rare values, some common ones that saturate.
        const auto code = random() % 4 == 0 ? static_cast<std::uint32_t>(random() % 40) : static_cast<std::uint32_t>(random());
        const auto& expected = saturated[code] ? std::vector<std::uint32_t>{} : reference[code];
        ASSERT_EQ(own.saturated(code), saturated[code]);
        own.places(code, found);
        ASSERT_EQ(found, expected) << place;
        if (saturated[code]) continue;
        own.add(code, place, found.size());
        if (found.size() >= seed_max_occurrences) {
            saturated[code] = true;
            reference.erase(code);
        } else {
            reference[code].push_back(place);
        }
    }
    EXPECT_LT(own.bytes(), 30000u * 8 + 4096 * 8 + 1024);
}

// Within 2 bits, OwnPlaces gives every earlier place within 2 bits, as a scan
// of all of them would, across merges.
TEST(OwnPlaces, FindsPlacesWithinTwoBitsAcrossMerges) {
    using comskip::fingerprint::OwnPlaces;
    using comskip::fingerprint::seed_max_occurrences;
    OwnPlaces own;
    std::vector<std::uint32_t> codes;
    std::map<std::uint32_t, bool> saturated;
    std::mt19937 random(9);
    std::vector<std::uint32_t> found;
    for (std::uint32_t place = 0; place < 12000; ++place) {
        // Some codes a bit or two off earlier ones, some near one common value.
        std::uint32_t code = random();
        if (place > 0 && random() % 3 == 0) code = codes[random() % codes.size()] ^ (1u << (random() % 32)) ^ (1u << (random() % 32));
        if (random() % 50 == 0) code = 0x1234abcd ^ (1u << (random() % 32));
        std::vector<std::uint32_t> expected;
        if (!saturated[code])
            for (std::uint32_t at = 0; at < codes.size(); ++at)
                if (std::popcount(codes[at] ^ code) <= 2 && !saturated[codes[at]]) expected.push_back(at);
        own.places(code, found);
        ASSERT_EQ(found, expected) << place;
        if (!saturated[code]) {
            own.add(code, place, found.size());
            if (found.size() >= seed_max_occurrences) saturated[code] = true;
        }
        codes.push_back(code);
    }
    // The common value's neighbours saturate.
    EXPECT_GT(std::ranges::count_if(saturated, [](const auto& item) { return item.second; }), 0);
}
}
