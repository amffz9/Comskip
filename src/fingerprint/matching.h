#pragma once
#include "fingerprint/coder.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace comskip::fingerprint {
// Two codes agree when at most this many of their 32 bits differ.
inline constexpr int agree_bits = 10;
// A repeat is at least 3 s of agreeing codes.
inline constexpr double min_repeat_seconds = 3.0;
// The two copies of a repeat inside one recording are at least 20 s apart.
inline constexpr double min_self_distance = 20.0;

// repeats.py: values seen more often than this (silence, a held tone) are not
// seeds.
inline constexpr std::size_t seed_max_occurrences = 30;
// Two codes are a seed when at most this many of their bits differ. Exact
// values failed to seed about 20% of the store's true repeats (the median pair
// shares 7.8% exact codes; a sub-hop phase between recordings alone lowers
// that); within 2 bits seeds 97%, with unrelated codes colliding at 3.3e-6.
inline constexpr int near_seed_bits = 2;
// Seeds on one offset closer than this form one group.
inline constexpr long seed_group_gap = 16;  // int(2 s / code period)
// The smoothing window: a position agrees when at least 5 of the 8 codes from 4
// before it to 3 after it agree.
inline constexpr long smooth_before = 4;
inline constexpr long smooth_after = 4;
inline constexpr int smooth_needed = 5;
// A match whose edge lies this close to a sequence's edge stopped there: the
// window cannot fill closer to the edge than this.
inline constexpr long edge_slack = 4;

bool codes_agree(std::uint32_t first, std::uint32_t second);

// The seeding rule and the quality gate; options so the Harness can measure
// exact seeding (seed_bits 0) or the matcher without its gate.
struct MatchRules {
    int seed_bits = near_seed_bits;
    bool quality_gate = true;
};

// Codes of several sequences, looked up by value within a few bits. Two codes
// within b bits differ in at most b / 2 bits of their high or of their low 16
// bits, so each half's places sit in buckets keyed by that half, and a lookup
// visits the buckets of every key within b / 2 bits of the code's halves.
// 12 bytes a code: the codes, and their places in each half's order.
class SeedIndex {
public:
    struct Place {
        std::uint32_t sequence{};
        std::int32_t position{};
        bool operator==(const Place&) const = default;
    };
    SeedIndex() = default;
    explicit SeedIndex(std::span<const std::span<const std::uint32_t>> sequences);
    void add(std::span<const std::uint32_t> codes);
    // Builds the lookup; called once, after the last add.
    void finish();

    std::size_t sequences() const { return starts_.size() - 1; }
    std::span<const std::uint32_t> codes(std::size_t sequence) const;
    // Every place whose code is within `bits` of the code, in (sequence,
    // position) order, or none once there are more than `most`.
    void near(std::uint32_t code, int bits, std::size_t most, std::vector<Place>& found) const;
    // How many places hold a code within `bits`, counted up to most + 1.
    std::size_t count(std::uint32_t code, int bits, std::size_t most) const;
    std::size_t bytes() const;

private:
    template <class Visit> bool visit(std::uint32_t code, int bits, Visit&& each) const;
    std::vector<std::uint32_t> codes_;
    std::vector<std::uint32_t> starts_{0};  // sequence i's codes are codes_[starts_[i], starts_[i + 1])
    std::vector<std::uint32_t> high_, low_;            // places sorted by the code's high, low 16 bits
    std::vector<std::uint32_t> high_from_, low_from_;  // where each key's places begin, 65537 each
};

// How well a stretch of agreeing codes agrees: the mean number of differing
// bits over its positions, the share of positions more than 6 bits apart, and
// whether two of its codes within 2 s of each other are equal (the stretch
// would have seeded with exact values).
struct MatchQuality {
    double mean_bits{};
    double loose_share{};
    bool exact_pair{};
    bool exact_code{};  // one of its codes equal to the other copy's
};
inline constexpr int loose_bits = 6;
// A stretch with no exact pair is a repeat only when it is at least 6 s long,
// its codes differ by at most 6 bits on average and at most half of them by
// more than 6 (step F4c, on the six labelled recordings: near-only matches over
// programme have a median of 7.2 bits and 0.64 over 6, 82% under 6 s; those
// over ads 5.7 bits and 0.36; matches with an exact pair 2.9 bits and 0.06).
inline constexpr double gate_min_seconds = 6.0;
inline constexpr double gate_mean_bits = 6.0;
inline constexpr double gate_loose_share = 0.5;
MatchQuality quality_of(std::span<const std::uint32_t> a, std::span<const std::uint32_t> b, long offset, long begin, long end);
// The quality gate (see matching.cpp): whether a stretch of `seconds` with this
// quality is a repeat.
bool passes_gate(const MatchQuality& quality, double seconds);

// One sequence of codes without a gap: a run of a recording, or an entry's
// reference copy. start is the time of code 0, used only to keep the copies of a
// self-repeat apart.
struct Sequence {
    std::span<const std::uint32_t> codes;
    double start{};
};

// Codes a_begin..a_end-1 of sequence a agree with b_begin.. of sequence b, at one
// offset. The flags say the match stopped at the edge of a sequence rather than
// where the codes stopped agreeing.
struct Match {
    std::size_t a{};
    long a_begin{};
    long a_end{};
    std::size_t b{};
    long b_begin{};
    long length() const { return a_end - a_begin; }
    long b_end() const { return b_begin + length(); }
    bool a_cut_at_start{}, a_cut_at_end{}, b_cut_at_start{}, b_cut_at_end{};
};

// Repeats between sequences a and b by the experiment's rule (repeats.py,
// October 27), seeded within rules.seed_bits: a code of a with 1 to 30 codes of
// b within that many bits (and at most 30 in a) seeds each pair; seeds on one
// offset less than 2 s apart form a group, and a group of at least two seeds is
// extended both ways while at least 5 of the 8 codes around a position agree;
// a stretch of at least 3 s that passes the quality gate is a repeat. Nothing
// is joined across sequences. With self set, a and b are the same list (pass b
// empty): only pairs whose copies are at least 20 s apart count, each once.
std::vector<Match> find_matches(std::span<const Sequence> a, std::span<const Sequence> b, bool self,
                                const MatchRules& rules = {});

long codes_for(double seconds);
}
