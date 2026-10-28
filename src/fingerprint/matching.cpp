#include "fingerprint/matching.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <tuple>
#include <unordered_map>

namespace comskip::fingerprint {
namespace {
struct Position {
    std::uint32_t sequence;
    std::int32_t index;
};

struct DiagonalKey {
    std::uint32_t a, b;
    std::int64_t offset;
    bool operator==(const DiagonalKey&) const = default;
};
struct DiagonalHash {
    std::size_t operator()(const DiagonalKey& key) const {
        auto value = static_cast<std::uint64_t>(key.offset) * 0x9e3779b97f4a7c15ull;
        value ^= (static_cast<std::uint64_t>(key.a) << 32 | key.b) + 0x632be59bd9b4e019ull + (value << 6) + (value >> 2);
        return static_cast<std::size_t>(value);
    }
};

// Every code of the sequences with where it is, sorted by code (then sequence
// and position): 12 bytes a code, where a map of vectors took about 100.
struct Placed {
    std::uint32_t code;
    Position at;
};
using Index = std::vector<Placed>;

Index index_of(std::span<const Sequence> sequences) {
    std::size_t total = 0;
    for (const auto& sequence : sequences) total += sequence.codes.size();
    Index index;
    index.reserve(total);
    for (std::uint32_t sequence = 0; sequence < sequences.size(); ++sequence) {
        const auto codes = sequences[sequence].codes;
        for (std::int32_t position = 0; position < static_cast<std::int32_t>(codes.size()); ++position)
            index.push_back({codes[static_cast<std::size_t>(position)], {sequence, position}});
    }
    std::ranges::sort(index, [](const Placed& x, const Placed& y) {
        return std::tie(x.code, x.at.sequence, x.at.index) < std::tie(y.code, y.at.sequence, y.at.index);
    });
    return index;
}

// The places of one code value.
std::span<const Placed> places(const Index& index, std::uint32_t code) {
    const auto [first, last] = std::ranges::equal_range(index, code, {}, &Placed::code);
    return {first, last};
}

class Diagonal {
public:
    Diagonal(std::span<const std::uint32_t> a, std::span<const std::uint32_t> b, long offset)
        : a_(a), b_(b), offset_(offset) {}

    bool good(long k) const {
        const long j = k + offset_;
        return k >= 0 && j >= 0 && k < static_cast<long>(a_.size()) && j < static_cast<long>(b_.size()) &&
               codes_agree(a_[static_cast<std::size_t>(k)], b_[static_cast<std::size_t>(j)]);
    }
    bool smooth(long k) const {
        int count = 0;
        for (long i = k - smooth_before; i < k + smooth_after; ++i) count += good(i) ? 1 : 0;
        return count >= smooth_needed;
    }

private:
    std::span<const std::uint32_t> a_, b_;
    long offset_;
};
}

bool codes_agree(std::uint32_t first, std::uint32_t second) { return std::popcount(first ^ second) <= agree_bits; }

long codes_for(double seconds) { return static_cast<long>(std::ceil(seconds / code_period - 1e-9)); }

std::vector<Match> find_matches(std::span<const Sequence> a, std::span<const Sequence> b, bool self) {
    const auto& other = self ? a : b;
    const long min_codes = codes_for(min_repeat_seconds);
    const long min_gap = static_cast<long>(min_self_distance / code_period);
    std::unordered_map<DiagonalKey, std::vector<std::int32_t>, DiagonalHash> seeds;

    const auto other_index = index_of(other);
    if (self) {
        for (std::size_t group = 0; group < other_index.size();) {
            std::size_t group_end = group + 1;
            while (group_end < other_index.size() && other_index[group_end].code == other_index[group].code) ++group_end;
            const std::span<const Placed> positions(other_index.begin() + static_cast<std::ptrdiff_t>(group),
                                                    other_index.begin() + static_cast<std::ptrdiff_t>(group_end));
            group = group_end;
            if (positions.size() < 2 || positions.size() > seed_max_occurrences) continue;
            for (std::size_t x = 0; x < positions.size(); ++x) {
                for (std::size_t y = x + 1; y < positions.size(); ++y) {
                    auto first = positions[x].at;
                    auto later = positions[y].at;
                    if (first.sequence > later.sequence) std::swap(first, later);
                    const auto& sa = a[first.sequence];
                    const auto& sb = a[later.sequence];
                    if (first.sequence == later.sequence) {
                        if (later.index - first.index < min_gap) continue;
                    } else if ((sb.start + later.index * code_period) - (sa.start + first.index * code_period) <
                               min_self_distance) {
                        continue;
                    }
                    seeds[{first.sequence, later.sequence, later.index - first.index}].push_back(first.index);
                }
            }
        }
    } else {
        const auto a_index = index_of(a);
        for (std::uint32_t sequence = 0; sequence < a.size(); ++sequence) {
            const auto codes = a[sequence].codes;
            for (std::int32_t position = 0; position < static_cast<std::int32_t>(codes.size()); ++position) {
                const auto code = codes[static_cast<std::size_t>(position)];
                const auto found = places(other_index, code);
                if (found.empty() || found.size() > seed_max_occurrences || places(a_index, code).size() > seed_max_occurrences)
                    continue;
                for (const auto& at : found) seeds[{sequence, at.at.sequence, at.at.index - position}].push_back(position);
            }
        }
    }

    // Visit diagonals in a fixed order so the result never depends on hashing.
    std::vector<std::pair<DiagonalKey, std::vector<std::int32_t>*>> diagonals;
    diagonals.reserve(seeds.size());
    for (auto& [key, starts] : seeds) diagonals.emplace_back(key, &starts);
    std::ranges::sort(diagonals, [](const auto& x, const auto& y) {
        return std::tie(x.first.a, x.first.b, x.first.offset) < std::tie(y.first.a, y.first.b, y.first.offset);
    });

    std::vector<Match> matches;
    for (const auto& [key, starts_pointer] : diagonals) {
        auto& starts = *starts_pointer;
        std::ranges::sort(starts);
        const auto codes_a = a[key.a].codes;
        const auto codes_b = other[key.b].codes;
        const long offset = static_cast<long>(key.offset);
        const Diagonal diagonal(codes_a, codes_b, offset);
        long covered_until = -1;  // end of the last match on this diagonal
        std::size_t group_begin = 0;
        while (group_begin < starts.size()) {
            std::size_t group_end = group_begin + 1;
            while (group_end < starts.size() && starts[group_end] - starts[group_end - 1] <= seed_group_gap) ++group_end;
            if (group_end - group_begin >= 2) {
                for (std::size_t seed = group_begin; seed < group_end; ++seed) {
                    const long k0 = starts[seed];
                    if (k0 < covered_until || !diagonal.smooth(k0)) continue;
                    long begin = k0;
                    while (diagonal.smooth(begin - 1)) --begin;
                    long end = k0 + 1;
                    while (diagonal.smooth(end)) ++end;
                    // The first copy of a self-repeat ends where the second begins.
                    if (self && key.a == key.b) end = std::min(end, begin + offset);
                    covered_until = end;
                    if (end - begin < min_codes) continue;
                    Match match{key.a, begin, end, key.b, begin + offset};
                    match.a_cut_at_start = begin <= edge_slack;
                    match.a_cut_at_end = end >= static_cast<long>(codes_a.size()) - edge_slack;
                    match.b_cut_at_start = match.b_begin <= edge_slack;
                    match.b_cut_at_end = match.b_end() >= static_cast<long>(codes_b.size()) - edge_slack;
                    matches.push_back(match);
                }
            }
            group_begin = group_end;
        }
    }
    return matches;
}
}
