// The Library's contract table (docs/fingerprint-plan.md, "PART: Library") on
// synthetic code streams: random codes with shared stretches copied in.
#include "fingerprint/library.h"
#include "fingerprint/matching.h"
#include "fingerprint/store.h"
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
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

Context plex_context(std::string series, double window = 3600) {
    auto result = context("", std::move(series), window);
    result.source = "plex";
    return result;
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
    EXPECT_NEAR(result.airings[0].start, 600 + edge_start, 1.0);
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
        // Kept as an intro either way; only step T4b marks it.
        EXPECT_EQ(statuses(), (std::map<std::string, int>{{"intro", 1}}));
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

TEST_F(LibraryTest, TheLiveIndexKeepsEveryAdNewestFirstWithinItsCap) {
    const auto old_spot = noise(13, codes_of(30.2));
    const auto new_spot = noise(14, codes_of(15.2));
    Recording first(60, 3600), second(61, 3600), third(62, 3600), fourth(63, 3600);
    first.put(900, old_spot);
    second.put(1900, old_spot);
    third.put(300, new_spot);
    fourth.put(1300, new_spot);
    const auto a = evidence("a", {});
    const auto b = evidence("b", {});
    const auto c = evidence("c", {});
    const auto d = evidence("d", {});
    const double then = now_2026 - 52 * week;
    add("a", first, context("CBS", "x", 3600, then), &a);
    add("b", second, context("NBC", "y", 3600, then + 3600), &b);
    add("c", third, context("ABC", "z", 3600, now_2026 - 7200), &c);
    add("d", fourth, context("FOX", "w", 3600, now_2026 - 3600), &d);
    ASSERT_EQ(with_status(Status::ad).size(), 2u);
    // A year old is no reason to leave the index: both, newest airing first.
    const auto index = library_->live_index();
    ASSERT_EQ(index.size(), 2u);
    EXPECT_NEAR(static_cast<double>(index[0].codes.size()), static_cast<double>(new_spot.size()), 3);
    EXPECT_NEAR(index[0].last_airing, now_2026 - 3600 + 1300, 2);
    EXPECT_NEAR(static_cast<double>(index[1].codes.size()), static_cast<double>(old_spot.size()), 3);
    // A cap with room for one keeps the entry aired most recently.
    std::vector<EntryInfo> capped;
    library_->live_index(live_index_entry_bytes(new_spot.size()) + 100, [&](EntryInfo&& entry) { capped.push_back(std::move(entry)); });
    ASSERT_EQ(capped.size(), 1u);
    EXPECT_EQ(capped[0].id, index[0].id);
    EXPECT_EQ(library_->live_index_key(live_index_entry_bytes(new_spot.size()) + 100).size(), 1u);
    EXPECT_EQ(library_->live_index_key(live_index_bytes).size(), 2u);
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

TEST_F(LibraryTest, ASpotInThreeSeriesIsAnAdWithoutEvidence) {
    const auto spot = noise(70, codes_of(30.2));
    const char* shows[] = {"show a", "show b", "show c"};
    for (int n = 0; n < 3; ++n) {
        Recording file(71 + n, 3600);
        file.put(1500 + 200 * n, spot);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
        // Two series are not enough without evidence.
        if (n == 1) EXPECT_EQ(statuses(), (std::map<std::string, int>{{"candidate", 1}}));
    }
    const auto ads = with_status(Status::ad);
    ASSERT_EQ(ads.size(), 1u);
    EXPECT_EQ(ads[0].reason, "in 3 series, no logo seen, an ad's length");
    EXPECT_EQ(library_->matches("plex:/2.ts").size(), 1u);
}

TEST_F(LibraryTest, OneSeriesAtOneSpotNeverMakesAnAd) {
    // Audio in the same spot of every episode is the show's own, however often
    // it recurs.
    const auto cue = noise(75, codes_of(30.2));
    for (int n = 0; n < 5; ++n) {
        Recording file(76 + n, 3600);
        file.put(1500 + 40 * n, cue);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
    }
    EXPECT_TRUE(with_status(Status::ad).empty());
}

TEST_F(LibraryTest, AnAdWithinOneSeriesAtVaryingPositionsIsAnAd) {
    // A 30 s spot in three episodes of one show, ten minutes apart: not a theme,
    // credits or recap, so an ad. Two episodes are not enough.
    const auto spot = noise(85, codes_of(30.2));
    const double at[] = {900, 1500, 2400};
    for (int n = 0; n < 3; ++n) {
        Recording file(86 + n, 3600);
        file.put(at[n], spot);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
        if (n == 1) EXPECT_TRUE(with_status(Status::ad).empty());
    }
    const auto ads = with_status(Status::ad);
    ASSERT_EQ(ads.size(), 1u);
    EXPECT_EQ(ads[0].reason, "in 3 recordings of show a at varying positions, no logo seen, an ad's length");
}

TEST_F(LibraryTest, ARecapOrPreviewWithinOneSeriesIsNotAnAd) {
    // A scene plays mid-episode, then in the next episodes' "previously on" (the
    // first minutes) and in a "next week on" (the last minutes): it varies in
    // position, but only one airing is away from the ends of its programme.
    const auto scene = noise(90, codes_of(30.2));
    const double at[] = {2000, 40, 100, 3520};
    for (int n = 0; n < 4; ++n) {
        Recording file(91 + n, 3600);
        file.put(at[n], scene);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
    }
    EXPECT_TRUE(with_status(Status::ad).empty());
}

TEST_F(LibraryTest, AnAdTwiceInOneRecordingIsAnAd) {
    // A 30 s spot twice in one game, an hour apart: a replay would have its own
    // commentary, so it is an ad. Ten seconds of crowd noise twice is not an
    // ad's length, and a spot twice within five minutes proves nothing.
    const auto spot = noise(200, codes_of(30.2));
    const auto close = noise(201, codes_of(15.2));
    const auto crowd = noise(202, codes_of(10));
    Recording game(203, 7200);
    game.put(1200, spot).put(4800, spot).put(2000, close).put(2200, close).put(3000, crowd).put(5000, crowd);
    add("game", game, context("NBC", "nfl:nbc", 7200), nullptr);
    std::map<double, EntryInfo> by_length;
    for (auto& entry : library_->entries()) by_length[std::round(entry.length)] = std::move(entry);
    ASSERT_TRUE(by_length.contains(30));
    EXPECT_EQ(by_length.at(30).status, Status::ad);
    EXPECT_EQ(by_length.at(30).reason, "twice in one recording, minutes apart, no logo seen, an ad's length");
    EXPECT_EQ(by_length.size(), 3u);
    EXPECT_EQ(with_status(Status::ad).size(), 1u);
}

TEST_F(LibraryTest, ALogoOverAPackageAiredTwiceKeepsItOutOfTheAds) {
    // A sponsored halftime package aired twice with the logo on screen.
    const auto package = noise(205, codes_of(30.2));
    Recording game(206, 7200);
    game.put(2400, package).put(5400, package);
    const auto logo = evidence("game", {}, {{0, 7200}});
    add("game", game, context("NBC", "nfl:nbc", 7200), &logo);
    EXPECT_TRUE(with_status(Status::ad).empty());
}

TEST_F(LibraryTest, OpeningCreditsAfterColdOpensOfVaryingLengthAreNotAnAd) {
    // Credits over the first act, six to twelve minutes in, and once later: the
    // position varies, but stays near the start in most airings.
    const auto credits = noise(100, codes_of(30.2));
    const double at[] = {380, 520, 700, 2300};
    for (int n = 0; n < 4; ++n) {
        Recording file(101 + n, 3600);
        file.put(at[n], credits);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
    }
    EXPECT_TRUE(with_status(Status::ad).empty());
}

TEST_F(LibraryTest, ABumperBesideAdsInTheSameSpotStaysAnIntro) {
    // A 4 s network bumper right after the first break of every episode, near
    // the start: not an ad's length, so it stays the show's own.
    const auto bumper = noise(105, codes_of(4));
    const auto ad = noise(106, codes_of(30.2));
    for (int n = 0; n < 3; ++n) {
        Recording file(107 + n, 3600);
        file.put(570, ad).put(600.2, bumper);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
    }
    const char* others[] = {"show b", "show c", "show d", "show e"};
    for (int n = 0; n < 4; ++n) {
        Recording file(110 + n, 3600);
        file.put(1800 + 300 * n, ad);
        add("plex:/other" + std::to_string(n) + ".ts", file, plex_context(others[n]), nullptr);
    }
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"ad", 1}, {"intro", 1}}));
}

TEST_F(LibraryTest, PiecesOfAnAdWithinOneSeriesChainToAnAdsLength) {
    // A 22 s body in four episodes of one show at varying positions, with an
    // 8 s tag in three of them: neither is an ad's length, together they are.
    const auto body = noise(95, codes_of(22));
    const auto tag = noise(96, codes_of(8.2));
    const auto other = noise(97, codes_of(8.2));
    const double at[] = {700, 1300, 2100, 2700};
    for (int n = 0; n < 4; ++n) {
        Recording file(98 + n, 3600);
        file.put(at[n], body).put(at[n] + 22, n == 3 ? other : tag);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
    }
    std::map<double, EntryInfo> by_length;
    for (auto& entry : library_->entries()) by_length[std::round(entry.length)] = std::move(entry);
    ASSERT_TRUE(by_length.contains(22));
    ASSERT_EQ(by_length.size(), 2u);
    EXPECT_EQ(by_length.at(22).status, Status::ad);
    EXPECT_EQ(by_length.at(22).reason, "in 4 recordings of show a at varying positions, no logo seen, 30 s with the airings it abuts");
    EXPECT_EQ(by_length.begin()->second.status, Status::ad);  // the tag
}

TEST_F(LibraryTest, ASponsorSpotInTheSameSpotEveryWeekIsAnAdBesideAds) {
    // A sponsor's 30 s spot first in the first break of three episodes, right
    // after an ad that airs in four other shows too: by position it would be
    // the show's intro, but a theme is bounded by programme, not by ads.
    const auto sponsor = noise(110, codes_of(30.2));
    const auto ad = noise(111, codes_of(30.2));
    for (int n = 0; n < 3; ++n) {
        Recording file(112 + n, 3600);
        file.put(570, ad).put(600.2, sponsor);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
    }
    const char* others[] = {"show b", "show c", "show d", "show e"};
    for (int n = 0; n < 4; ++n) {
        Recording file(115 + n, 3600);
        file.put(1800 + 300 * n, ad);
        add("plex:/other" + std::to_string(n) + ".ts", file, plex_context(others[n]), nullptr);
    }
    const auto ads = with_status(Status::ad);
    ASSERT_EQ(ads.size(), 2u);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"ad", 2}}));
    const bool named = std::ranges::any_of(ads, [](const EntryInfo& entry) {
        return entry.reason == "intro by position, but next to ads in 3 recordings";
    });
    EXPECT_TRUE(named);
}

TEST_F(LibraryTest, AThemeBoundedByProgrammeStaysAnIntro) {
    // The same, with programme around the theme: it stays the show's intro.
    const auto theme = noise(120, codes_of(30.2));
    for (int n = 0; n < 3; ++n) {
        Recording file(121 + n, 3600);
        file.put(600, theme);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show a"), nullptr);
    }
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"intro", 1}}));
}

TEST_F(LibraryTest, AnAdEarlyInTwoEpisodesOfOneShowIsNotItsIntro) {
    // Two airings near the start of one show come first: an intro then. Three
    // more series make the show hold less than half the airings, so it is an ad.
    const auto spot = noise(80, codes_of(30.2));
    Recording first(81, 3600), second(82, 3600);
    first.put(400, spot);
    second.put(700, spot);
    add("plex:/a1.ts", first, plex_context("show a"), nullptr);
    add("plex:/a2.ts", second, plex_context("show a"), nullptr);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"intro", 1}}));
    const char* shows[] = {"show b", "show c", "show d"};
    for (int n = 0; n < 3; ++n) {
        Recording file(83 + n, 3600);
        file.put(1500 + 300 * n, spot);
        add("plex:/o" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
    }
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"ad", 1}}));
}

TEST_F(LibraryTest, ALogoSeenInOneAiringVetoesTheAudioOnlyAd) {
    const auto clip = noise(90, codes_of(30.2));
    const char* shows[] = {"show a", "show b", "show c"};
    for (int n = 0; n < 3; ++n) {
        Recording file(91 + n, 3600);
        file.put(1500, clip);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
    }
    ASSERT_EQ(with_status(Status::ad).size(), 1u);
    // A capture whose Comskip run saw its logo over the whole airing.
    Recording capture(95, 3600);
    capture.put(2000, clip);
    const auto copy = evidence("c", {}, {{1900, 2100}});
    add("c", capture, context("CBS", "show d", 3600), &copy);
    const auto shown = with_status(Status::in_programme_ad);
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_TRUE(with_status(Status::ad).empty());
}

TEST_F(LibraryTest, AStretchNextToAdsInTwoBroadcastsJoinsTheBreak) {
    // A 30 s spot in four series; a 7 s tag right after it in files 0 and 1, an
    // 8 s stretch right after it in files 2 and 3 (each two series, an odd
    // length). Both join the spot's break.
    const auto spot = noise(100, codes_of(30.2));
    const auto tag = noise(101, codes_of(7));
    const auto other = noise(102, codes_of(8));
    const char* shows[] = {"show a", "show b", "show c", "show d"};
    for (int n = 0; n < 4; ++n) {
        Recording file(103 + n, 3600);
        const double at = 1500 + 100 * n;
        file.put(at, spot).put(at + 30.2, n < 2 ? tag : other);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
    }
    std::map<double, EntryInfo> by_length;
    for (auto& entry : library_->entries()) by_length[std::round(entry.length)] = std::move(entry);
    EXPECT_EQ(by_length.at(30).status, Status::ad);
    EXPECT_EQ(by_length.at(7).status, Status::ad);
    EXPECT_EQ(by_length.at(7).reason, "next to ads in 2 broadcasts");
    EXPECT_EQ(by_length.at(8).status, Status::ad);
}

TEST_F(LibraryTest, PiecesOfAnAdJudgedTogetherWhereTheyAbut) {
    // A 30 s spot whose last 8 s differ in one airing (another end tag): the
    // entry splits into a 22 s body and an 8 s tag, neither an ad's length. In
    // the airings where they abut, together they are 30 s, so both are ads.
    const auto body = noise(150, codes_of(22));
    const auto tag = noise(151, codes_of(8.2));
    const auto other_tag = noise(152, codes_of(8.2));
    const char* shows[] = {"show a", "show b", "show c", "show d"};
    for (int n = 0; n < 4; ++n) {
        Recording file(153 + n, 3600);
        const double at = 1500 + 100 * n;
        file.put(at, body).put(at + 22, n == 3 ? other_tag : tag);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
    }
    const auto entries = library_->entries();
    ASSERT_EQ(entries.size(), 2u);
    for (const auto& entry : entries) {
        EXPECT_FALSE(audio_ad_length(entry.length)) << entry.length;
        EXPECT_EQ(entry.status, Status::ad) << entry.length;
        if (entry.length > 15) EXPECT_EQ(entry.reason, "in 4 series, no logo seen, 30 s with the airings it abuts");
    }
}

TEST_F(LibraryTest, PiecesThatAddUpInTwoRecordingsAloneAreNotAnAd) {
    // Two captures on two channels share 31 s of a game (a highlight replayed in
    // another broadcast) that the Library holds as 13 s and 17 s pieces: an
    // ad's length together, but in two recordings only and next to no ad.
    const auto first = noise(160, codes_of(13.2));
    const auto second = noise(161, codes_of(17));
    const char* channels[] = {"NBC", "32.1"};
    for (int n = 0; n < 2; ++n) {
        Recording file(162 + n, 3600);
        file.put(1500 + 700 * n, first).put(1513.2 + 700 * n, second);
        add("capture" + std::to_string(n), file, context(channels[n], n == 0 ? "nfl:nbc" : "college", 3600), nullptr);
    }
    // Split them: each piece also airs alone in a third recording.
    Recording third(164, 3600);
    third.put(900, first).put(2000, second);
    add("capture2", third, context("CBS", "news", 3600), nullptr);
    EXPECT_TRUE(with_status(Status::ad).empty());
}

TEST_F(LibraryTest, EveryVersionOfAnAdWithOneBodyIsAnAd) {
    // One 22 s body with three end tags (8, 23 and 38 s: a 30, a 45 and a 60 s
    // version), each version in three shows. The body splits from every tag;
    // no piece is an ad's length alone, and in every airing the body and its
    // tag make one, so every piece of every version is an ad.
    const auto body = noise(170, codes_of(22));
    const std::vector<std::pair<double, std::vector<uint32_t>>> tags = {
        {30, noise(171, codes_of(8.2))}, {45, noise(172, codes_of(23.2))}, {60, noise(173, codes_of(38.2))}};
    for (int n = 0; n < 9; ++n) {
        Recording file(174 + n, 3600);
        const double at = 1500 + 100 * n;
        file.put(at, body).put(at + 22, tags[n % 3].second);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context("show " + std::to_string(n)), nullptr);
    }
    std::map<double, EntryInfo> by_length;
    for (auto& entry : library_->entries()) by_length[std::round(entry.length)] = std::move(entry);
    ASSERT_EQ(by_length.size(), 4u);
    EXPECT_EQ(by_length.at(22).status, Status::ad);
    EXPECT_EQ(by_length.at(22).airings, 9);
    for (const auto& [version, tag] : {std::pair{30, 8}, std::pair{45, 23}, std::pair{60, 38}}) {
        SCOPED_TRACE(version);
        ASSERT_TRUE(by_length.contains(tag));
        EXPECT_EQ(by_length.at(tag).status, Status::ad);
        EXPECT_EQ(by_length.at(tag).reason,
                  "in 3 series, no logo seen, " + std::to_string(version) + " s with the airings it abuts");
    }
}

TEST_F(LibraryTest, AStretchNextToAnAdInOneBroadcastStaysCandidate) {
    const auto spot = noise(110, codes_of(30.2));
    const auto tag = noise(111, codes_of(7));
    const char* shows[] = {"show a", "show b", "show c"};
    for (int n = 0; n < 3; ++n) {
        Recording file(112 + n, 3600);
        file.put(1500, spot);
        // The tag follows the spot in file 0; in file 1 it airs elsewhere.
        if (n == 0) file.put(1530.2, tag);
        if (n == 1) file.put(2500, tag);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
    }
    for (const auto& entry : library_->entries())
        if (std::round(entry.length) == 7) EXPECT_EQ(entry.status, Status::candidate);
}

TEST_F(LibraryTest, JoiningNeverTakesOneSeriesAudio) {
    // A show's own 7 s sting after the spot, in two episodes of that show only.
    const auto spot = noise(120, codes_of(30.2));
    const auto sting = noise(121, codes_of(7));
    const char* shows[] = {"show a", "show a", "show b", "show c"};
    for (int n = 0; n < 4; ++n) {
        Recording file(122 + n, 3600);
        file.put(1500 + 100 * n, spot);
        if (n < 2) file.put(1530.2 + 100 * n, sting);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
    }
    for (const auto& entry : library_->entries())
        if (std::round(entry.length) == 7) EXPECT_NE(entry.status, Status::ad);
}

TEST_F(LibraryTest, EveryGameOfASportsSeriesIsItsOwnBroadcast) {
    // Plex files of one series named for their air date, one game each: a spot
    // in three games is in three broadcasts. Episodes of a show are not.
    const auto spot = noise(140, codes_of(30.2));
    const char* games[] = {"2026-09-13 07 00 00 - Bears at Panthers", "2026-09-20 07 00 00 - Rams at Broncos",
                           "2026-09-27 07 00 00 - Bills at Texans"};
    for (int n = 0; n < 3; ++n) {
        Recording file(141 + n, 3600);
        file.put(1500 + 10 * n, spot);
        add(std::string("plex:/NFL Football - ") + games[n] + ".ts", file, plex_context("title:nfl football"), nullptr);
    }
    const auto ads = with_status(Status::ad);
    ASSERT_EQ(ads.size(), 1u);
    EXPECT_EQ(ads[0].reason, "in 3 series, no logo seen, an ad's length");
}

TEST_F(LibraryTest, TwoFilesOfOneBroadcastCountAsOneSeries) {
    // File b runs into the next show: it shares 600 s of that show with file c,
    // spot included. The spot is then in two series, not three.
    const auto spot = noise(130, codes_of(30.2));
    const auto shared = noise(131, codes_of(600));
    Recording a(132, 3600), b(133, 5400), c(134, 3600);
    a.put(1500, spot);
    b.put(3700, shared).put(4100, spot);
    c.put(100, shared).put(500, spot);
    add("plex:/a.ts", a, plex_context("show a"), nullptr);
    add("plex:/b.ts", b, plex_context("show b", 5400), nullptr);
    add("plex:/c.ts", c, plex_context("show c"), nullptr);
    for (const auto& entry : library_->entries())
        if (std::round(entry.length) == 30) EXPECT_EQ(entry.status, Status::candidate);
}

TEST_F(LibraryTest, TwoCapturesOfOneBroadcastCountAsOneChannel) {
    // The same game captured twice, the channel named "CBS" in one and "4.1" in
    // the other: a spot in it and one other series is in two series on one
    // channel, so not an ad by recurrence alone.
    const auto spot = noise(160, codes_of(30.2));
    const auto game = noise(161, codes_of(900));
    Recording first(162, 3600), second(163, 3600), other(164, 3600);
    first.put(1000, game).put(1500, spot);
    second.put(1000, game).put(1500, spot);
    other.put(2000, spot);
    add("plex:/other.ts", other, plex_context("show a"), nullptr);
    add("first", first, context("CBS", "nfl:cbs", 3600), nullptr);
    add("second", second, context("4.1", "series:nfl", 3600), nullptr);
    for (const auto& entry : library_->entries())
        if (std::round(entry.length) == 30) EXPECT_EQ(entry.status, Status::candidate) << entry.reason;
}

TEST_F(LibraryTest, PromoteAllAppliesTheRulesAgain) {
    const auto spot = noise(140, codes_of(30.2));
    const char* shows[] = {"show a", "show b", "show c"};
    for (int n = 0; n < 3; ++n) {
        Recording file(141 + n, 3600);
        file.put(1500, spot);
        add("plex:/" + std::to_string(n) + ".ts", file, plex_context(shows[n]), nullptr);
    }
    // An entry made programme by the intro rule before T4b, as older stores hold.
    store_->execute("UPDATE entry SET status = 'programme', "
                    "reason = 'near the start of show a in every airing (intro rule off until T4b)'");
    const auto result = library_->promote_all();
    EXPECT_EQ(result.promoted, 1);
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"ad", 1}}));
    // A programme entry for any other reason stays.
    store_->execute("UPDATE entry SET status = 'programme', reason = 'logo in every airing, one series'");
    library_->promote_all();
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"programme", 1}}));
    // The thresholds are options: four series needed, it stays candidate.
    store_->execute("UPDATE entry SET status = 'candidate', reason = ''");
    LibraryOptions strict;
    strict.rules.audio_series = 4;
    Library(*store_, strict).promote_all();
    EXPECT_EQ(statuses(), (std::map<std::string, int>{{"candidate", 1}}));
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
    // One channel, so the audio-only rule (three series, or two on two channels)
    // stays out of these.
    AiringFacts free_b{"b", "nfl:nbc", "CBS", true, false, false, 0.0, false, false};
    std::vector<AiringFacts> across{free_a, free_b};
    EXPECT_EQ(promote(Status::candidate, 30, across).status, Status::ad);
    EXPECT_EQ(promote(Status::candidate, 33, across).status, Status::ad);  // seams at both ends in a
    across[0].seam_end = false;
    EXPECT_EQ(promote(Status::candidate, 33, across).status, Status::candidate);
    // Programme, intro and credits never change.
    EXPECT_EQ(promote(Status::programme, 30, across).status, Status::programme);
    // Without evidence no airing shows that the logo is absent.
    across[0].has_evidence = across[1].has_evidence = false;
    EXPECT_EQ(promote(Status::candidate, 30, across).status, Status::candidate);
    // One airing alone is not a repeat.
    EXPECT_EQ(promote(Status::candidate, 30, std::vector<AiringFacts>{free_a}).status, Status::candidate);
    // An ad lost its rule when a later airing shows a logo.
    std::vector<AiringFacts> logo{free_a, free_b};
    logo[1].logo_fraction = 0.3;
    EXPECT_EQ(promote(Status::ad, 30, logo).status, Status::candidate);
}

TEST(Promotion, AudioOnlyRules) {
    const auto plex = [](std::string recording, std::string series) {
        AiringFacts facts;
        facts.recording = std::move(recording);
        facts.series_key = std::move(series);
        return facts;
    };
    std::vector<AiringFacts> three{plex("a", "x"), plex("b", "y"), plex("c", "z")};
    EXPECT_EQ(promote(Status::candidate, 30.8, three).status, Status::ad);
    EXPECT_EQ(promote(Status::candidate, 15.0, three).status, Status::ad);
    // 10 and 20 s are not lengths the audio-only rule takes; the stretch is shown
    // as an in-programme ad and may join a break.
    const auto twenty = promote(Status::candidate, 20, three);
    EXPECT_EQ(twenty.status, Status::in_programme_ad);
    EXPECT_TRUE(twenty.may_join);
    EXPECT_EQ(promote(Status::candidate, 31.2, three).status, Status::in_programme_ad);
    // Two series need two channels as well.
    std::vector<AiringFacts> two{plex("a", "x"), plex("b", "y")};
    EXPECT_EQ(promote(Status::candidate, 30, two).status, Status::candidate);
    EXPECT_TRUE(promote(Status::candidate, 30, two).may_join);
    two[0].channel = "4.1";
    two[1].channel = "5.1";
    EXPECT_EQ(promote(Status::candidate, 30, two).status, Status::ad);
    // One broadcast in two files counts once.
    three[2].broadcast = "y";
    EXPECT_EQ(promote(Status::candidate, 30, three).status, Status::candidate);
    // A threshold of 2 series takes it.
    PromotionRules loose;
    loose.audio_series = 2;
    EXPECT_EQ(promote(Status::candidate, 30, three, loose).status, Status::ad);
    // One series never joins a break; neither does anything longer than an ad.
    std::vector<AiringFacts> one{plex("a", "x"), plex("b", "x")};
    EXPECT_FALSE(promote(Status::candidate, 7, one).may_join);
    EXPECT_FALSE(promote(Status::candidate, 200, std::vector<AiringFacts>{plex("a", "x"), plex("b", "y")}).may_join);
    // A logo seen in an airing with evidence vetoes the ad, and joining.
    std::vector<AiringFacts> seen{plex("a", "x"), plex("b", "y"), plex("c", "z")};
    seen[0].has_evidence = true;
    seen[0].logo_fraction = 0.6;
    const auto vetoed = promote(Status::candidate, 30, seen);
    EXPECT_EQ(vetoed.status, Status::in_programme_ad);
    EXPECT_FALSE(vetoed.may_join);
    seen[0].logo_fraction = 0.2;
    EXPECT_EQ(promote(Status::candidate, 30, seen).status, Status::candidate);
    // An ad that joined a break and lost it is looked at like any candidate.
    EXPECT_EQ(promote(Status::ad, 7, std::vector<AiringFacts>{plex("a", "x"), plex("b", "y")}).status, Status::candidate);
}

TEST(Promotion, AnIntroNeedsItsSeriesToHoldMostAirings) {
    AiringFacts early{"a", "x", "", false, false, false, 0.0, true, false};
    auto early_b = early;
    early_b.recording = "b";
    std::vector<AiringFacts> airings{early, early_b};
    EXPECT_EQ(promote(Status::candidate, 30, airings).status, Status::intro);
    for (const char* name : {"c", "d", "e"}) {
        AiringFacts other{name, std::string("s-") + name, "", false, false, false, 0.0, false, false};
        airings.push_back(other);
    }
    // x holds 2 of 5 recordings: not its intro, and an intro entry returns to
    // the rules (here an ad) instead of becoming programme.
    EXPECT_EQ(promote(Status::candidate, 30, airings).status, Status::ad);
    EXPECT_EQ(promote(Status::intro, 30, airings).status, Status::ad);
    // While the series holds most airings, one away from the start makes the
    // intro programme.
    auto mid = early;
    mid.recording = "f";
    mid.near_start = false;
    std::vector<AiringFacts> broken{early, early_b, mid};
    EXPECT_EQ(promote(Status::intro, 30, broken).status, Status::programme);
}

// Flips `bits` distinct random bits of every code.
std::vector<std::uint32_t> flipped(std::vector<std::uint32_t> codes, int bits, unsigned seed) {
    std::mt19937 random(seed);
    for (auto& code : codes) {
        std::uint32_t mask = 0;
        while (std::popcount(mask) < bits) mask |= 1u << (random() % 32);
        code ^= mask;
    }
    return codes;
}

TEST(SeedIndex, FindsEveryCodeWithinTheBitsOnce) {
    std::mt19937 random(5);
    std::vector<std::vector<std::uint32_t>> sequences(3);
    for (auto& codes : sequences)
        for (int k = 0; k < 4000; ++k) codes.push_back(random());
    // Neighbours of one code at every distance up to 4 bits, in both halves.
    const std::uint32_t code = 0x5a3c0ff1;
    for (const std::uint32_t mask : {0u, 1u, 1u << 20, 3u, 3u << 15, 1u << 31 | 1u, 7u << 14, 15u, 5u << 17 | 5u})
        sequences[1][100 + std::popcount(mask) * 7 + (mask & 0xff) % 5] = code ^ mask;
    std::vector<std::span<const std::uint32_t>> spans(sequences.begin(), sequences.end());
    const SeedIndex index(spans);
    std::vector<SeedIndex::Place> found;
    for (int bits = 0; bits <= 5; ++bits) {
        for (const std::uint32_t probe : {code, static_cast<std::uint32_t>(random()), sequences[2][17]}) {
            std::vector<SeedIndex::Place> expected;
            for (std::uint32_t s = 0; s < sequences.size(); ++s)
                for (std::int32_t k = 0; k < static_cast<std::int32_t>(sequences[s].size()); ++k)
                    if (std::popcount(sequences[s][static_cast<std::size_t>(k)] ^ probe) <= bits) expected.push_back({s, k});
            index.near(probe, bits, 1000, found);
            EXPECT_EQ(found, expected) << bits;
            EXPECT_EQ(index.count(probe, bits, 1000), expected.size());
        }
    }
    // More places than allowed: none, and the count stops just past the limit.
    index.near(code, 4, 3, found);
    EXPECT_TRUE(found.empty());
    EXPECT_EQ(index.count(code, 4, 3), 4u);
    EXPECT_EQ(index.codes(1).size(), 4000u);
    EXPECT_LT(index.bytes(), 12000u * 12 + 2 * 65537 * 4 + 64);
}

TEST(Matching, NearSeedsFindARepeatWithNoEqualCodes) {
    const auto clip = noise(95, codes_of(12));
    auto first = noise(96, codes_of(100));
    auto second = noise(97, codes_of(100));
    std::copy(clip.begin(), clip.end(), first.begin() + codes_of(30));
    const auto changed = flipped(clip, 2, 98);  // every code 2 bits off
    std::copy(changed.begin(), changed.end(), second.begin() + codes_of(60));
    const std::vector<Sequence> a{{first, 0}};
    const std::vector<Sequence> b{{second, 0}};
    EXPECT_TRUE(find_matches(a, b, false, {0, true}).empty());
    const auto matches = find_matches(a, b, false);
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_NEAR(matches[0].a_begin, codes_of(30), 2);
    EXPECT_NEAR(matches[0].length(), static_cast<long>(clip.size()), 4);
    EXPECT_EQ(matches[0].b_begin - matches[0].a_begin, codes_of(60) - codes_of(30));
    // The same inside one recording.
    auto self = noise(99, codes_of(200));
    std::copy(clip.begin(), clip.end(), self.begin() + codes_of(20));
    std::copy(changed.begin(), changed.end(), self.begin() + codes_of(120));
    const std::vector<Sequence> own{{self, 0}};
    EXPECT_TRUE(find_matches(own, {}, true, {0, true}).empty());
    EXPECT_EQ(find_matches(own, {}, true).size(), 1u);
}

TEST(Matching, TheQualityGateKeepsLooseAndShortNearOnlyStretchesOut) {
    const auto clip = noise(100, codes_of(12));
    auto first = noise(101, codes_of(100));
    std::copy(clip.begin(), clip.end(), first.begin() + codes_of(30));
    const std::vector<Sequence> a{{first, 0}};
    const auto with = [&](const std::vector<std::uint32_t>& copy, long count, const MatchRules& rules = {}) {
        auto second = noise(102, codes_of(100));
        std::copy_n(copy.begin(), count, second.begin() + codes_of(50));
        const std::vector<Sequence> b{{second, 0}};
        return find_matches(a, b, false, rules).size();
    };
    // Seeds within 2 bits, but most codes 7 bits apart: agreement by the
    // smoothing rule alone is no repeat.
    auto loose = flipped(clip, 7, 103);
    for (std::size_t k = 0; k < loose.size(); k += 6) loose[k] = flipped({clip[k]}, 2, static_cast<unsigned>(k))[0];
    EXPECT_EQ(with(loose, static_cast<long>(loose.size()), {near_seed_bits, false}), 1u);
    EXPECT_EQ(with(loose, static_cast<long>(loose.size())), 0u);
    // 2 bits apart throughout: kept at 12 s, not at 5 s with no equal codes...
    const auto near = flipped(clip, 2, 104);
    EXPECT_EQ(with(near, static_cast<long>(near.size())), 1u);
    EXPECT_EQ(with(near, codes_of(5)), 0u);
    EXPECT_EQ(with(near, codes_of(5), {near_seed_bits, false}), 1u);
    // ... while 5 s that would have seeded exactly is kept.
    auto exact = near;
    exact[10] = clip[10];
    exact[18] = clip[18];
    EXPECT_EQ(with(exact, codes_of(5)), 1u);
}

TEST(Matching, QualityOfAStretch) {
    const std::vector<std::uint32_t> a{0, 0, 0, 0, 0xff, 0};
    const std::vector<std::uint32_t> b{0, 1, 0, 0x7f, 0, 0};
    // Positions 1..5 of a against 0..4 of b: 0^0, 0^1, 0^0, 0xff^0x7f, 0^0.
    const auto quality = quality_of(a, b, -1, 1, 6);
    EXPECT_DOUBLE_EQ(quality.mean_bits, 2.0 / 5);
    EXPECT_DOUBLE_EQ(quality.loose_share, 0.0);
    EXPECT_TRUE(quality.exact_pair);
    EXPECT_TRUE(quality.exact_code);
    EXPECT_FALSE(quality_of(a, b, -1, 2, 3).exact_code);
    EXPECT_TRUE(passes_gate({7.0, 0.9, true}, 3));
    EXPECT_FALSE(passes_gate({2.0, 0.1, false}, 5.9));
    EXPECT_TRUE(passes_gate({6.0, 0.5, false}, 6));
    EXPECT_FALSE(passes_gate({6.1, 0.2, false}, 20));
    EXPECT_FALSE(passes_gate({4.0, 0.6, false}, 20));
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
