#pragma once
#include "fingerprint/coder.h"
#include "fingerprint/inputs.h"
#include "fingerprint/store.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// Library (docs/fingerprint-plan.md, "PART: Library"): the store of code streams,
// entries and airings; finds repeats and promotes entries. Sole owner of the
// store's contents.
namespace comskip::fingerprint {
enum class Status { candidate, ad, in_programme_ad, intro, credits, programme };
std::string_view status_name(Status status);
Status status_from_name(std::string_view name);
// programme, intro and credits never become an ad (programme never changes; an
// intro or credits entry an airing breaks the rule for becomes programme).
bool status_is_final(Status status);

// Where the audio of a run of agreeing codes lies. Code i of a run starting at S
// begins at S + i * code_period and covers 2.72 s of audio; codes whose audio is
// mostly shared still agree, so a stretch of agreeing codes begin..end-1 is taken
// as the audio from S + begin * period + edge_start to S + end * period +
// edge_end. Measured against Comskip's seams on g5 and g4 (step F4): with both
// at 1.2 s, starts fell 0.5-0.9 s after the seam and ends 0.2-0.4 s after it.
inline constexpr double edge_start = 0.60;
inline constexpr double edge_end = 0.90;

// Logo seen during less than this share of an airing counts as no logo.
inline constexpr double logo_none_below = 0.10;
// An in-programme ad has a logo during at least this share of every airing.
inline constexpr double logo_in_programme = 0.50;
// A seam this close to an airing's edge bounds it.
inline constexpr double seam_reach = 1.0;
// Lengths an ad is sold in, matched within 1 s.
bool standard_length(double seconds);

// What promotion needs to know about one airing, from its own recording.
struct AiringFacts {
    std::string recording;
    std::string series_key;  // empty when unknown
    std::string channel;     // empty when unknown
    bool has_evidence{};     // its recording's evidence is stored and learned a logo
    bool seam_start{}, seam_end{};
    double logo_fraction{};  // share of the airing with a logo; meaningful only with evidence
    bool near_start{}, near_end{};
};
struct Promotion {
    Status status{};
    std::string reason;
};
// The promotion rules, in the contract's order, over all of an entry's airings.
// With intro_credits off (until step T4b) an entry the intro or credits rule
// picks becomes programme.
Promotion promote(Status current, double length, std::span<const AiringFacts> airings, bool intro_credits);

struct Airing {
    long long entry{};
    std::string recording;
    double run_start{};
    long code_begin{}, code_end{};
    long part_begin{}, part_end{};
    bool cut_at_start{}, cut_at_end{};
    std::string found_by;
    std::optional<bool> seam_start, seam_end;
    std::string logo = "unknown";  // yes, no or unknown
    std::optional<double> logo_fraction;
    double start() const { return run_start + code_begin * code_period + edge_start; }
    double end() const { return run_start + code_end * code_period + edge_end; }
};

struct EntryInfo {
    long long id{};
    Status status{};
    double length{};
    std::string reason;
    long long airings{};
    std::vector<std::uint32_t> codes;
    double last_airing{};
};

// One stretch for the Combiner: an airing of an entry whose status is marked.
struct LibraryMatch {
    double start{}, end{};
    Status status{};
    long long entry{};
};

struct RepeatsResult {
    std::vector<LibraryMatch> airings;           // this recording's marked airings
    std::vector<long long> changed_entries;      // status changed, split, or new and not candidate
    std::vector<std::string> corrected;          // other captures whose marks may change
    int promoted{};                              // entries that left candidate
    long long airings_found{};                   // all airings this search added
};

struct LibraryOptions {
    bool intro_credits = false;  // step T4b
    std::string found_by = "pass";
};

class Library {
public:
    explicit Library(Store& store, LibraryOptions options = {}) : store_(store), options_(std::move(options)) {}

    // Stores the stream; more runs for the recording extend it, the same run
    // again changes nothing, and a run that grew replaces its shorter copy.
    void add_stream(const std::string& recording, const Context& context, std::span<const CodeRun> runs);
    // Requires the stream to be stored. Evidence, when given, is kept for the
    // recording and used for its airings.
    RepeatsResult find_repeats(const std::string& recording, const Evidence* evidence);
    // Entries with status ad or in-programme ad whose last airing is within 8
    // weeks of now (Unix seconds), most recently aired first, at most 20 MB of
    // codes.
    std::vector<EntryInfo> live_index(double now);
    // The same, at most max_bytes of codes, each entry handed to visit in turn
    // so the whole list is never held at once.
    void live_index(double now, std::size_t max_bytes, const std::function<void(EntryInfo&&)>& visit);
    // What the live index holds (id, status, code count), read without the codes,
    // so a caller can tell cheaply whether it changed.
    std::vector<std::tuple<long long, Status, long long>> live_index_key(double now, std::size_t max_bytes);

    bool has_stream(const std::string& recording);
    // Marked airings of a recording (ad and in-programme ad; intro and credits too
    // once step T4b is on).
    std::vector<LibraryMatch> matches(const std::string& recording);
    // Every entry, for measurement.
    std::vector<EntryInfo> entries();
    // Every airing of a recording, any status, for measurement.
    std::vector<Airing> airings(const std::string& recording);

private:
    Store& store_;
    LibraryOptions options_;
};

inline constexpr double live_index_weeks = 8;
inline constexpr std::size_t live_index_bytes = 20u << 20;
}
