#pragma once
#include "fingerprint/coder.h"
#include "fingerprint/library.h"
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
// The live index in memory: every entry's codes in one array and their places
// sorted by code value, 8 bytes per code. Built away from the matching thread,
// then shared read-only.
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
    // Every (entry, position) holding the code, or none when the code is held in
    // more than seed_max_occurrences places.
    void hits(std::uint32_t code, std::vector<std::pair<std::size_t, long>>& found) const;

private:
    std::vector<std::uint32_t> codes_;
    std::vector<std::uint32_t> starts_{0};  // entry i's codes are codes_[starts_[i], starts_[i + 1])
    std::vector<long long> ids_;
    std::vector<Status> statuses_;
    std::vector<std::uint32_t> order_;  // places in codes_, sorted by the code there
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
    LiveMatcher();
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
    bool end_track(Track& track, double now);
    void update(Track& track);
    std::span<const std::uint32_t> reference(const Track& track) const;
    bool smooth(const Track& track, long position) const;
    double completed(long position) const;

    std::shared_ptr<const LiveIndex> index_;
    std::vector<OwnRun> runs_;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> own_;  // code -> places in the recording
    std::unordered_map<DiagonalKey, Diagonal, DiagonalHash> diagonals_;
    std::vector<Track> tracks_;
    std::vector<LiveMatch> matches_;
    std::vector<std::pair<std::size_t, long>> hits_;
    bool changed_{};
};
}
