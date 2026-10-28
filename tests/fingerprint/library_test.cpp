// The Library's contract table (docs/fingerprint-plan.md, "PART: Library") on
// synthetic code streams: random codes with shared stretches copied in.
#include "fingerprint/library.h"
#include "fingerprint/matching.h"
#include "fingerprint/store.h"
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <thread>

namespace {
using namespace comskip::fingerprint;
namespace fs = std::filesystem;

constexpr double week = 7 * 24 * 3600.0;
constexpr double now_2026 = 1792000000;  // late 2026, Unix seconds

std::vector<std::uint32_t> noise(unsigned seed, long count) {
    std::mt19937 random(seed);
    std::vector<std::uint32_t> codes(static_cast<std::size_t>(count));
    for (auto& code : codes) code = random();
    return codes;
}
long codes_of(double seconds) { return static_cast<long>(std::lround(seconds / code_period)); }
// The time where a stretch copied in at code k begins or ends as the Library
// sees it (the smoothing window drops one code at the start).
double time_of(long k) { return k * code_period + edge_start; }

// A recording: noise with clips copied in at given seconds.
struct Recording {
    std::vector<std::uint32_t> codes;
    Recording(unsigned seed, double seconds) : codes(noise(seed, codes_of(seconds))) {}
    Recording& put(double at, const std::vector<std::uint32_t>& clip, long count = -1) {
        const auto first = static_cast<std::size_t>(codes_of(at));
        const auto size = count < 0 ? clip.size() : static_cast<std::size_t>(count);
        std::copy_n(clip.begin(), size, codes.begin() + static_cast<std::ptrdiff_t>(first));
        return *this;
    }
    std::vector<CodeRun> runs() const { return {{0.0, codes}}; }
};

Context context(std::string channel, std::string series, double window, double air_start = now_2026) {
    Context result;
    result.channel = std::move(channel);
    result.air_start = air_start;
    result.programmes.push_back({0, window, "show", std::move(series)});
    return result;
}

// Evidence with seams at the given times and logo over the given intervals.
Evidence evidence(std::string id, std::vector<double> seams, std::vector<Interval> logo = {}) {
    Evidence result;
    result.capture_id = std::move(id);
    result.final = true;
    result.logo_known = true;
    for (const double at : seams) result.seams.push_back({at, {"black"}});
    result.logo = std::move(logo);
    return result;
}
// Seams at both edges of a clip of `seconds` copied in at `at`.
std::vector<double> edges(double at, double seconds) {
    const long first = codes_of(at);
    return {time_of(first) - 0.3, first * code_period + seconds + edge_end - code_period - 0.2};
}

class LibraryTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        directory_ = fs::temp_directory_path() / (std::string("comskip-library-") + info->name());
        fs::remove_all(directory_);
        fs::create_directories(directory_);
        path_ = (directory_ / "store.sqlite").string();
        store_.emplace(path_);
        library_.emplace(*store_);
    }
    void TearDown() override {
        library_.reset();
        store_.reset();
        std::error_code ignored;
        fs::remove_all(directory_, ignored);
    }
    RepeatsResult add(const std::string& id, const Recording& recording, const Context& context, const Evidence* evidence) {
        library_->add_stream(id, context, recording.runs());
        return library_->find_repeats(id, evidence);
    }
    std::map<std::string, int> statuses() {
        std::map<std::string, int> counts;
        for (const auto& entry : library_->entries()) ++counts[std::string(status_name(entry.status))];
        return counts;
    }
    std::vector<EntryInfo> with_status(Status status) {
        std::vector<EntryInfo> found;
        for (auto& entry : library_->entries())
            if (entry.status == status) found.push_back(std::move(entry));
        return found;
    }

    fs::path directory_;
    std::string path_;
    std::optional<Store> store_;
    std::optional<Library> library_;
};

TEST_F(LibraryTest, ThirtySecondRepeatsBoundedBySeamsBecomeAds) {
    const auto spot = noise(1, codes_of(30.2));
    const auto sting = noise(2, codes_of(2.5));
    Recording game(10, 5400);
    game.put(600, spot).put(2400, spot).put(1000, sting).put(3000, sting);
    auto seams = edges(600, 30.2);
    const auto more = edges(2400, 30.2);
    seams.insert(seams.end(), more.begin(), more.end());
    const auto copy = evidence("g5", seams);
    const auto result = add("g5", game, context("NBC", "nfl:nbc", 12600), &copy);
    const auto ads = with_status(Status::ad);
    ASSERT_EQ(ads.size(), 1u);
    EXPECT_NEAR(ads[0].length, 30.2, 0.5);
    EXPECT_EQ(ads[0].airings, 2);
    EXPECT_EQ(result.promoted, 1);
    ASSERT_EQ(result.airings.size(), 2u);
    EXPECT_NEAR(result.airings[0].start, 600, 1.0);
    EXPECT_NEAR(result.airings[1].end, 2430.2, 1.0);
    // The 2.5 s sting is shorter than a repeat (3 s): never an entry, never an ad.
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"ad", 1}}));
    EXPECT_TRUE(result.corrected.empty());
}

TEST_F(LibraryTest, ARepeatInAnotherSeriesCorrectsTheEarlierCapture) {
    const auto spot = noise(1, codes_of(30.2));
    const auto other = noise(3, codes_of(20.0));
    Recording g5(10, 5400), g4(11, 5400);
    g5.put(700, spot).put(1500, other);
    g4.put(900, spot);
    const auto five = evidence("g5", {});
    const auto four = evidence("g4", {});
    add("g5", g5, context("NBC", "nfl:nbc", 12600), &five);
    EXPECT_TRUE(with_status(Status::ad).empty());
    const auto result = add("g4", g4, context("CBS", "nfl:cbs", 10800), &four);
    const auto ads = with_status(Status::ad);
    ASSERT_EQ(ads.size(), 1u);
    EXPECT_EQ(ads[0].reason, "across shows, no logo, standard length");
    EXPECT_EQ(result.corrected, std::vector<std::string>{"g5"});
    EXPECT_EQ(library_->matches("g5").size(), 1u);
    EXPECT_EQ(library_->matches("g4").size(), 1u);
}

TEST_F(LibraryTest, ASharedGraphicWithTheLogoIsProgramme) {
    const auto graphic = noise(4, codes_of(12));
    Recording first(10, 5400), second(11, 5400);
    first.put(800, graphic);
    second.put(1800, graphic);
    const auto a = evidence("a", edges(800, 12), {{700, 900}});
    const auto b = evidence("b", edges(1800, 12), {{1700, 1900}});
    add("a", first, context("CBS", "nfl:cbs", 10800), &a);
    add("b", second, context("CBS", "nfl:cbs", 10800), &b);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"programme", 1}}));
}

TEST_F(LibraryTest, NineSecondsWithoutSeamsStaysCandidate) {
    const auto clip = noise(5, codes_of(9));
    Recording first(10, 5400), second(11, 5400);
    first.put(800, clip);
    second.put(1800, clip);
    const auto a = evidence("a", {});
    const auto b = evidence("b", {});
    add("a", first, context("CBS", "nfl:cbs", 10800), &a);
    add("b", second, context("CBS", "nfl:cbs", 10800), &b);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"candidate", 1}}));
}

TEST_F(LibraryTest, AnAdUnderTheLogoAcrossSeriesIsAnInProgrammeAd) {
    const auto clip = noise(6, codes_of(31));
    Recording first(10, 5400), second(11, 5400);
    first.put(1478, clip);
    second.put(4233, clip);
    const auto a = evidence("a", {}, {{1400, 1600}});
    const auto b = evidence("b", {}, {{4200, 4300}});
    add("a", first, context("NBC", "nfl:nbc", 12600), &a);
    add("b", second, context("CBS", "nfl:cbs", 10800), &b);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"in-programme ad", 1}}));
    ASSERT_EQ(library_->matches("b").size(), 1u);
    EXPECT_EQ(library_->matches("b")[0].status, Status::in_programme_ad);
}

TEST_F(LibraryTest, AFifteenSecondCutSplitsAThirtySecondEntry) {
    const auto spot = noise(7, codes_of(30.2));
    Recording first(10, 5400), second(11, 5400);
    first.put(600, spot).put(3000, spot);
    // The 15 s cut shares the spot's first half; its second half is other audio.
    second.put(2000, spot, codes_of(15.1));
    const auto a = evidence("a", {});
    const auto b = evidence("b", {});
    add("a", first, context("NBC", "nfl:nbc", 12600), &a);
    ASSERT_EQ(library_->entries().size(), 1u);
    EXPECT_NEAR(library_->entries()[0].length, 30.2, 0.5);
    const auto result = add("b", second, context("CBS", "nfl:cbs", 10800), &b);
    const auto entries = library_->entries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_NEAR(entries[0].length, 15, 0.6);
    EXPECT_NEAR(entries[1].length, 15, 0.6);
    // The shared half has three airings, the rest two.
    EXPECT_EQ(entries[0].airings + entries[1].airings, 5);
    EXPECT_EQ(result.corrected, std::vector<std::string>{"a"});
    EXPECT_EQ(result.changed_entries.size(), 2u);
}

TEST_F(LibraryTest, AThemeNearTheStartOfEveryEpisodeIsAnIntro) {
    for (const bool on : {false, true}) {
        TearDown();
        SetUp();
        library_.emplace(*store_, LibraryOptions{on, "pass"});
        const auto theme = noise(8, codes_of(40));
        for (int episode = 0; episode < 3; ++episode) {
            // The theme starts 2-4 min in, after cold opens that differ.
            Recording show(20 + episode, 3600);
            show.put(120 + 60 * episode, theme);
            const auto copy = evidence("e" + std::to_string(episode), {});
            add("e" + std::to_string(episode), show, context("CBS", "elsbeth", 3600), &copy);
        }
        EXPECT_EQ(statuses(), (std::map<std::string, int>{{std::string(status_name(on ? Status::intro : Status::programme)), 1}}));
        const auto marks = library_->matches("e1");
        if (on) {
            ASSERT_EQ(marks.size(), 1u);
            EXPECT_NEAR(marks[0].start, 180, 1.0);
            EXPECT_NEAR(marks[0].end, 220, 1.0);
        } else {
            EXPECT_TRUE(marks.empty());
        }
    }
}

TEST_F(LibraryTest, CreditsBoundedBySeamsAreCreditsNotAds) {
    library_.emplace(*store_, LibraryOptions{true, "pass"});
    const auto credits = noise(10, codes_of(30.2));
    for (int episode = 0; episode < 3; ++episode) {
        Recording show(30 + episode, 3600);
        show.put(3600 - 60 - 30.2, credits);
        const auto copy = evidence("e" + std::to_string(episode), edges(3600 - 60 - 30.2, 30.2));
        add("e" + std::to_string(episode), show, context("CBS", "elsbeth", 3600), &copy);
    }
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"credits", 1}}));
}

TEST_F(LibraryTest, AThemeOnceMidEpisodeIsNotAnIntro) {
    library_.emplace(*store_, LibraryOptions{true, "pass"});
    const auto theme = noise(11, codes_of(40));
    const double at[] = {150, 200, 1800};
    for (int episode = 0; episode < 3; ++episode) {
        Recording show(40 + episode, 3600);
        show.put(at[episode], theme);
        const auto copy = evidence("e" + std::to_string(episode), {});
        add("e" + std::to_string(episode), show, context("CBS", "elsbeth", 3600), &copy);
    }
    // After two episodes it was an intro; the third airing, mid-episode, makes it
    // programme, never an ad.
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"programme", 1}}));
}

TEST_F(LibraryTest, PlexFilesAloneStayCandidates) {
    const auto spot = noise(12, codes_of(30.2));
    Recording first(50, 3600), second(51, 3600);
    first.put(900, spot);
    second.put(1900, spot);
    auto plex = context("", "show a", 3600);
    plex.source = "plex";
    auto other = context("", "show b", 3600);
    other.source = "plex";
    add("plex:/a.ts", first, plex, nullptr);
    add("plex:/b.ts", second, other, nullptr);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"candidate", 1}}));
    // An airing with evidence and no logo then makes it an ad; Plex files are
    // never corrected captures.
    Recording third(52, 3600);
    third.put(2500, spot);
    const auto copy = evidence("c", {});
    const auto result = add("c", third, context("CBS", "show c", 3600), &copy);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"ad", 1}}));
    EXPECT_TRUE(result.corrected.empty());
}

TEST_F(LibraryTest, TheLiveIndexHoldsAdsAiredInTheLastEightWeeks) {
    const auto spot = noise(13, codes_of(30.2));
    Recording first(60, 3600), second(61, 3600);
    first.put(900, spot);
    second.put(1900, spot);
    const auto a = evidence("a", {});
    const auto b = evidence("b", {});
    const double then = now_2026 - 10 * week;
    add("a", first, context("CBS", "x", 3600, then), &a);
    add("b", second, context("NBC", "y", 3600, then + 3600), &b);
    ASSERT_EQ(with_status(Status::ad).size(), 1u);
    EXPECT_TRUE(library_->live_index(now_2026).empty());
    EXPECT_EQ(library_->live_index(then + 7200).size(), 1u);
    // Found again by a pass today: back in the index.
    Recording third(62, 3600);
    third.put(300, spot);
    const auto c = evidence("c", {});
    add("c", third, context("ABC", "z", 3600, now_2026 - 3600), &c);
    const auto index = library_->live_index(now_2026);
    ASSERT_EQ(index.size(), 1u);
    EXPECT_NEAR(index[0].last_airing, now_2026 - 3600 + 300, 2);
}

TEST_F(LibraryTest, TheSameStreamTwiceChangesNothing) {
    const auto spot = noise(14, codes_of(30.2));
    Recording game(70, 5400);
    game.put(600, spot).put(2400, spot);
    const auto copy = evidence("g", {});
    add("g", game, context("NBC", "nfl:nbc", 12600), &copy);
    const auto before = library_->entries();
    const auto airings = library_->airings("g").size();
    const auto again = add("g", game, context("NBC", "nfl:nbc", 12600), &copy);
    EXPECT_EQ(again.airings_found, 0);
    EXPECT_EQ(library_->airings("g").size(), airings);
    ASSERT_EQ(library_->entries().size(), before.size());
    EXPECT_EQ(library_->entries()[0].codes, before[0].codes);
    // A run that grew replaces its shorter copy; one that didn't is ignored.
    Recording longer = game;
    longer.codes.push_back(7);
    library_->add_stream("g", context("NBC", "nfl:nbc", 12600), longer.runs());
    library_->add_stream("g", context("NBC", "nfl:nbc", 12600), game.runs());
    auto query = store_->prepare("SELECT length(codes) / 4 FROM run WHERE recording = 'g'");
    ASSERT_TRUE(query.step());
    EXPECT_EQ(query.integer(0), static_cast<long long>(longer.codes.size()));
}

TEST_F(LibraryTest, WaitsForAnotherWriter) {
    sqlite3* other = nullptr;
    ASSERT_EQ(sqlite3_open(path_.c_str(), &other), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(other, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr), SQLITE_OK);
    std::atomic<bool> released{false};
    std::thread holder([&] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        sqlite3_exec(other, "COMMIT", nullptr, nullptr, nullptr);
        released = true;
    });
    const auto started = std::chrono::steady_clock::now();
    EXPECT_NO_THROW(library_->add_stream("w", context("CBS", "x", 3600), Recording(80, 60).runs()));
    EXPECT_TRUE(released);
    EXPECT_GE(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(1500));
    holder.join();
    sqlite3_close(other);
    EXPECT_TRUE(library_->has_stream("w"));
}

TEST(Store, AnUnreadableFileIsCorruptAndANewerOneTooNew) {
    const auto directory = fs::temp_directory_path() / "comskip-store-errors";
    fs::remove_all(directory);
    fs::create_directories(directory);
    const auto garbage = (directory / "garbage.sqlite").string();
    std::ofstream(garbage, std::ios::binary) << std::string(4096, 'x');
    EXPECT_THROW(Store{garbage}, StoreCorrupt);
    const auto newer = (directory / "newer.sqlite").string();
    { Store store(newer); store.execute("UPDATE meta SET value = '99' WHERE key = 'schema'"); }
    EXPECT_THROW(Store{newer}, StoreTooNew);
    fs::remove_all(directory);
}

TEST(Promotion, RulesInOrder) {
    AiringFacts free_a{"a", "nfl:cbs", "CBS", true, true, true, 0.0, false, false};
    AiringFacts free_b{"b", "nfl:nbc", "NBC", true, false, false, 0.0, false, false};
    std::vector<AiringFacts> across{free_a, free_b};
    EXPECT_EQ(promote(Status::candidate, 30, across, false).status, Status::ad);
    EXPECT_EQ(promote(Status::candidate, 33, across, false).status, Status::ad);  // seams at both ends in a
    across[0].seam_end = false;
    EXPECT_EQ(promote(Status::candidate, 33, across, false).status, Status::candidate);
    // Programme, intro and credits never change.
    EXPECT_EQ(promote(Status::programme, 30, across, false).status, Status::programme);
    // Without evidence no airing shows that the logo is absent.
    across[0].has_evidence = across[1].has_evidence = false;
    EXPECT_EQ(promote(Status::candidate, 30, across, false).status, Status::candidate);
    // One airing alone is not a repeat.
    EXPECT_EQ(promote(Status::candidate, 30, std::vector<AiringFacts>{free_a}, false).status, Status::candidate);
    // An ad lost its rule when a later airing shows a logo.
    std::vector<AiringFacts> logo{free_a, free_b};
    logo[1].logo_fraction = 0.3;
    EXPECT_EQ(promote(Status::ad, 30, logo, false).status, Status::candidate);
}

TEST(Matching, NeverJoinsAcrossRunsAndKeepsCopiesTwentySecondsApart) {
    const auto clip = noise(90, codes_of(10));
    auto first = noise(91, codes_of(100));
    auto second = noise(92, codes_of(100));
    std::copy(clip.begin(), clip.begin() + codes_of(5), first.end() - codes_of(5));
    std::copy(clip.begin() + codes_of(5), clip.end(), second.begin());
    auto third = noise(93, codes_of(200));
    std::copy(clip.begin(), clip.end(), third.begin() + codes_of(150));
    // The clip split across two runs matches the whole clip in another
    // sequence only as two pieces, one per run.
    const std::vector<Sequence> runs{{first, 0}, {second, 100}};
    const std::vector<Sequence> other{{third, 0}};
    const auto matches = find_matches(runs, other, false);
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].a, 0u);
    EXPECT_TRUE(matches[0].a_cut_at_end);
    EXPECT_EQ(matches[1].a, 1u);
    EXPECT_TRUE(matches[1].a_cut_at_start);
    // Within one sequence a copy 10 s later is not a repeat; one 30 s later is.
    auto self = noise(94, codes_of(200));
    std::copy(clip.begin(), clip.end(), self.begin() + codes_of(20));
    std::copy(clip.begin(), clip.end(), self.begin() + codes_of(30));
    const std::vector<Sequence> close{{self, 0}};
    EXPECT_TRUE(find_matches(close, {}, true).empty());
    std::copy(clip.begin(), clip.end(), self.begin() + codes_of(60));
    const std::vector<Sequence> far{{self, 0}};
    EXPECT_FALSE(find_matches(far, {}, true).empty());
}
}
