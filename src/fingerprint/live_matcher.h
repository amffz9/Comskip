#pragma once
#include "fingerprint/coder.h"
#include "fingerprint/library.h"
#include "fingerprint/matching.h"
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

// LiveMatcher (docs/fingerprint-plan.md, "PART: LiveMatcher"): matches a growing
// code stream against the live index and against the recording's own earlier
// codes.
namespace comskip::fingerprint {
// The live index in memory: every entry's codes in a SeedIndex, 12 bytes per
// code. Built away from the matching thread, then shared read-only.
class LiveIndex {
public:
    void add(long long id, Status status, std::span<const std::uint32_t> codes);
    // Sorts the lookup; called once, after the last add.
    void finish();

    std::size_t size() const { return ids_.size(); }
    long long id(std::size_t entry) const { return ids_[entry]; }
    Status status(std::size_t entry) const { return statuses_[entry]; }
    std::span<const std::uint32_t> codes(std::size_t entry) const;
    std::size_t bytes() const;
    // Every (entry, position) holding a code within `bits` of the code, or none
    // when more than seed_max_occurrences places do.
    void hits(std::uint32_t code, int bits, std::vector<SeedIndex::Place>& found) const;

private:
    SeedIndex codes_;
    std::vector<long long> ids_;
    std::vector<Status> statuses_;
};

// The recording's own codes for self-repeat seeds: every place (code number in
// the recording) of each code value, in the order added, until a value has
// seed_max_occurrences places within `bits` of it; then it is saturated and
// never a seed again. Flat (code, place) pairs, 8 bytes each, sorted (which
// orders them by the code's high 16 bits too) with the pairs' order by their
// low 16 bits, 4 bytes each; and a recent part merged into them every
// recent_limit pairs.
class OwnPlaces {
public:
    explicit OwnPlaces(int bits = near_seed_bits) : bits_(bits) {}
    // The places held for codes within `bits` of the code, oldest first, or
    // none once the code is saturated.
    void places(std::uint32_t code, std::vector<std::uint32_t>& found) const;
    bool saturated(std::uint32_t code) const;
    // After looking at places: adds the place, or saturates the code when it is
    // already held in seed_max_occurrences places.
    void add(std::uint32_t code, std::uint32_t place, std::size_t held);
    std::size_t bytes() const;

private:
    struct Pair {
        std::uint32_t code, place;
        auto operator<=>(const Pair&) const = default;
    };
    static constexpr std::size_t recent_limit = 4096;
    void merge();
    int bits_;
    std::vector<Pair> sorted_;
    std::vector<std::uint32_t> low_;  // places in sorted_, by the low 16 bits of their code
    std::vector<Pair> recent_;              // unsorted, in the order added
    std::vector<std::uint32_t> saturated_;  // sorted
};

// More than 1 s of codes that don't agree inside an extent ends a match: the
// tenth such code comes 9 code periods (1.11 s) after the first.
inline constexpr long divergence_codes = 10;

// A match in the recording, in capture seconds.
struct LiveMatch {
    long long entry{};  // the live index entry; 0 for a repeat of the recording's own earlier codes
    Status status{};    // the entry's status; candidate for a self-repeat
    double from{}, to{};                // the whole extent here: agreed part plus projected part
    double agreed_from{}, agreed_to{};  // where the codes have agreed
    double earlier_from{}, earlier_to{};  // a self-repeat's earlier copy
    bool ended{};      // grows no more: its extent ended, it diverged, or its run ended
    bool withdrawn{};  // ended before its extent did, so its projected part was withdrawn
    // The capture second reached (the end of the newest code's audio) when the
    // codes last stopped agreeing, and when the match ended.
    double stopped_at{-1}, ended_at{-1};
    bool self() const { return entry == 0; }
    bool projected() const { return from < agreed_from || to > agreed_to; }
};

class LiveMatcher {
public:
    explicit LiveMatcher(MatchRules rules = {});
    ~LiveMatcher();
    LiveMatcher(const LiveMatcher&) = delete;
    LiveMatcher& operator=(const LiveMatcher&) = delete;

    // Later seeds use the new index; matches already under way keep their entry.
    void refresh(std::shared_ptr<const LiveIndex> index);
    // A new run starting at the capture second: the open matches end where their
    // codes agreed, and nothing is matched across the boundary.
    void run_start(double start);
    // The next codes of the current run (a run must have started). True when a
    // match is new or changed.
    bool feed(std::span<const std::uint32_t> codes);
    // The audio ended: the open matches end where their codes agreed.
    void finish();

    // Every match reported, in the order found. A match is reported once at
    // least 3 s of codes agree at one offset.
    const std::vector<LiveMatch>& matches() const { return matches_; }
    long long codes() const;
    // The recording's own codes and the lookup of them, in bytes.
    std::size_t own_bytes() const;
    // The live index in use, in bytes (0 before the first refresh).
    std::size_t index_bytes() const { return index_ ? index_->bytes() : 0; }

private:
    struct OwnRun {
        double start{};
        std::uint32_t first{};  // the place of its code 0 in the recording
        std::vector<std::uint32_t> codes;
    };
    // Seeds on one diagonal, the latest group of them.
    struct Diagonal {
        long first{}, last{};
        int count{};
    };
    // A diagonal being followed: waiting for its first agreeing position, then
    // growing while its codes agree.
    struct Track {
        long long source{};
        long offset{};
        Status status{};
        std::shared_ptr<const std::vector<std::uint32_t>> entry_codes;  // an index entry's reference copy
        std::size_t own_run{};                                           // a self-repeat's earlier run
        long seed{};       // the first seed; agreement is looked for from here
        long next{};       // the next position to look at
        long begin{-1}, end{-1};  // agreed positions [begin, end) once started
        std::size_t match = no_match;
        bool done{};
        bool complete{};  // agreed to the end of its reference copy, or of a self-repeat's first copy
    };
    static constexpr std::size_t no_match = static_cast<std::size_t>(-1);
    struct DiagonalKey {
        long long source;  // an entry id, or -(own run + 1)
        long offset;       // the reference position is the run position plus this
        bool operator==(const DiagonalKey&) const = default;
    };
    struct DiagonalHash {
        std::size_t operator()(const DiagonalKey& key) const;
    };

    void add_code(std::uint32_t code);
    void seed(const DiagonalKey& key, long position, Status status, std::span<const std::uint32_t> entry_codes);
    bool advance(Track& track, long newest);
    bool good_enough(const Track& track) const;
    bool end_track(Track& track, double now);
    void update(Track& track);
    std::span<const std::uint32_t> reference(const Track& track) const;
    bool smooth(const Track& track, long position) const;
    double completed(long position) const;

    MatchRules rules_;
    std::shared_ptr<const LiveIndex> index_;
    std::vector<OwnRun> runs_;
    OwnPlaces own_;
    std::vector<std::uint32_t> places_;  // scratch for own_.places
    std::unordered_map<DiagonalKey, Diagonal, DiagonalHash> diagonals_;
    std::vector<Track> tracks_;
    std::vector<LiveMatch> matches_;
    std::vector<SeedIndex::Place> hits_;
    bool changed_{};
};
}
