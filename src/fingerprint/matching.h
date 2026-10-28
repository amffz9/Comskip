#pragma once
#include "fingerprint/coder.h"
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

bool codes_agree(std::uint32_t first, std::uint32_t second);

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
// October 27): codes that occur 1 to 30 times on each side are exact-value
// seeds; seeds on one offset less than 2 s apart form a group, and a group of
// at least two seeds is extended both ways while at least 5 of the 8 codes
// around a position agree; a stretch of at least 3 s is a repeat. Nothing is
// joined across sequences. With self set, a and b are the same list (pass b
// empty): only pairs whose copies are at least 20 s apart count, each once.
std::vector<Match> find_matches(std::span<const Sequence> a, std::span<const Sequence> b, bool self);

long codes_for(double seconds);
}
