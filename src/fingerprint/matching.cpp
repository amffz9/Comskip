#include "fingerprint/matching.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <tuple>
#include <unordered_map>

namespace comskip::fingerprint {
namespace {
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

// Calls each(key) for every 16-bit key within `bits` of `key`, until it returns
// false.
template <class Each> bool keys_near(std::uint32_t key, int bits, int from, Each&& each) {
    if (!each(key)) return false;
    if (bits == 0) return true;
    for (int bit = from; bit < 16; ++bit)
        if (!keys_near(key ^ (1u << bit), bits - 1, bit + 1, each)) return false;
    return true;
}

// Places sorted by a 16-bit key of their code (a counting sort, so places stay
// in order within a key), with where each key's places begin, and the codes in
// that order.
void bucket(std::span<const std::span<const std::uint32_t>> sequences, std::size_t total, int shift,
            std::vector<std::uint32_t>& order, std::vector<std::uint32_t>& codes, std::vector<std::uint32_t>& from) {
    from.assign((1u << 16) + 1, 0);
    for (const auto sequence : sequences)
        for (const auto code : sequence) ++from[((code >> shift) & 0xffff) + 1];
    for (std::size_t key = 1; key < from.size(); ++key) from[key] += from[key - 1];
    order.resize(total);
    codes.resize(total);
    std::vector<std::uint32_t> next(from.begin(), from.end() - 1);
    std::uint32_t place = 0;
    for (const auto sequence : sequences)
        for (const auto code : sequence) {
            const auto at = next[(code >> shift) & 0xffff]++;
            order[at] = place++;
            codes[at] = code;
        }
}
}

SeedIndex::SeedIndex(std::span<const std::span<const std::uint32_t>> sequences)
    : sequences_(sequences.begin(), sequences.end()) {
    starts_.reserve(sequences.size() + 1);
    for (const auto codes : sequences) starts_.push_back(starts_.back() + static_cast<std::uint32_t>(codes.size()));
    finish();
}

void SeedIndex::add(std::span<const std::uint32_t> codes) {
    owned_.insert(owned_.end(), codes.begin(), codes.end());
    starts_.push_back(static_cast<std::uint32_t>(owned_.size()));
}

void SeedIndex::finish() {
    starts_.shrink_to_fit();
    // Built with add: the sequences are the copy, now that it no longer grows.
    if (sequences_.size() + 1 < starts_.size()) {
        owned_.shrink_to_fit();
        sequences_.clear();
        for (std::size_t sequence = 0; sequence + 1 < starts_.size(); ++sequence)
            sequences_.push_back(std::span<const std::uint32_t>(owned_).subspan(starts_[sequence], starts_[sequence + 1] - starts_[sequence]));
    }
    // The codes again in each order, so a lookup reads its buckets straight
    // through instead of jumping to every place's code.
    bucket(sequences_, starts_.back(), 16, high_, high_codes_, high_from_);
    bucket(sequences_, starts_.back(), 0, low_, low_codes_, low_from_);
}

std::span<const std::uint32_t> SeedIndex::codes(std::size_t sequence) const { return sequences_[sequence]; }

std::size_t SeedIndex::bytes() const {
    return (owned_.capacity() + starts_.capacity() + high_.capacity() + low_.capacity() + high_codes_.capacity() +
            low_codes_.capacity() + high_from_.capacity() + low_from_.capacity()) *
               sizeof(std::uint32_t) +
           sequences_.capacity() * sizeof(std::span<const std::uint32_t>);
}

// Calls each(place) for every place within `bits` of the code, each once: a
// place whose high half is within bits / 2 is found through the high buckets,
// any other through the low ones (its low half is then within bits / 2).
template <class Visit> bool SeedIndex::visit(std::uint32_t code, int bits, Visit&& each) const {
    if (high_from_.empty()) return true;
    const int half = bits / 2;
    const auto high = [&](std::uint32_t key) {
        for (auto at = high_from_[key]; at < high_from_[key + 1]; ++at)
            if (std::popcount(high_codes_[at] ^ code) <= bits && !each(high_[at])) return false;
        return true;
    };
    const auto low = [&](std::uint32_t key) {
        for (auto at = low_from_[key]; at < low_from_[key + 1]; ++at) {
            const auto differ = low_codes_[at] ^ code;
            if (std::popcount(differ) <= bits && std::popcount(differ >> 16) > half && !each(low_[at])) return false;
        }
        return true;
    };
    return keys_near(code >> 16, half, 0, high) && keys_near(code & 0xffff, half, 0, low);
}

void SeedIndex::near(std::uint32_t code, int bits, std::size_t most, std::vector<Place>& found) const {
    found.clear();
    const bool all = visit(code, bits, [&](std::uint32_t place) {
        if (found.size() == most) return false;
        const auto sequence = static_cast<std::uint32_t>(std::ranges::upper_bound(starts_, place) - starts_.begin() - 1);
        found.push_back({sequence, static_cast<std::int32_t>(place - starts_[sequence])});
        return true;
    });
    if (!all) {
        found.clear();
        return;
    }
    std::ranges::sort(found, [](const Place& x, const Place& y) { return std::tie(x.sequence, x.position) < std::tie(y.sequence, y.position); });
}

std::size_t SeedIndex::count(std::uint32_t code, int bits, std::size_t most) const {
    std::size_t counted = 0;
    visit(code, bits, [&](std::uint32_t) { return ++counted <= most; });
    return counted;
}

void SeedIndex::all_near(std::uint32_t code, int bits, std::vector<std::uint32_t>& found) const {
    found.clear();
    visit(code, bits, [&](std::uint32_t place) {
        found.push_back(place);
        return true;
    });
}

std::uint32_t SeedIndex::sequence_of(std::uint32_t place) const {
    return static_cast<std::uint32_t>(std::ranges::upper_bound(starts_, place) - starts_.begin() - 1);
}

namespace {
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

MatchQuality quality_of(std::span<const std::uint32_t> a, std::span<const std::uint32_t> b, long offset, long begin, long end) {
    MatchQuality quality;
    long bits = 0, loose = 0, last_exact = -1, count = 0;
    for (long k = begin; k < end; ++k) {
        const long j = k + offset;
        if (k < 0 || j < 0 || k >= static_cast<long>(a.size()) || j >= static_cast<long>(b.size())) continue;
        const int differ = std::popcount(a[static_cast<std::size_t>(k)] ^ b[static_cast<std::size_t>(j)]);
        ++count;
        bits += differ;
        loose += differ > loose_bits ? 1 : 0;
        if (differ == 0) {
            quality.exact_code = true;
            if (last_exact >= 0 && k - last_exact <= seed_group_gap) quality.exact_pair = true;
            last_exact = k;
        }
    }
    if (count > 0) {
        quality.mean_bits = static_cast<double>(bits) / static_cast<double>(count);
        quality.loose_share = static_cast<double>(loose) / static_cast<double>(count);
    }
    return quality;
}

bool passes_gate(const MatchQuality& quality, double seconds) {
    return quality.exact_pair || (seconds >= gate_min_seconds - 1e-9 && quality.mean_bits <= gate_mean_bits &&
                                  quality.loose_share <= gate_loose_share);
}

namespace {
using Seeds = std::unordered_map<DiagonalKey, std::vector<std::int32_t>, DiagonalHash>;

SeedIndex index_of(std::span<const Sequence> sequences) {
    std::vector<std::span<const std::uint32_t>> codes;
    codes.reserve(sequences.size());
    for (const auto& sequence : sequences) codes.push_back(sequence.codes);
    return SeedIndex(codes);
}

// Groups each diagonal's seeds and extends the groups into matches.
std::vector<Match> extend_seeds(Seeds& seeds, std::span<const Sequence> a, std::span<const Sequence> other, bool self,
                                const MatchRules& rules) {
    const long min_codes = codes_for(min_repeat_seconds);
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
                    if (rules.quality_gate &&
                        !passes_gate(quality_of(codes_a, codes_b, offset, begin, end), static_cast<double>(end - begin) * code_period))
                        continue;
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

std::vector<Match> find_matches(std::span<const Sequence> a, std::span<const Sequence> b, bool self, const MatchRules& rules) {
    const auto& other = self ? a : b;
    const long min_gap = static_cast<long>(min_self_distance / code_period);
    Seeds seeds;

    const auto other_index = index_of(other);
    // Own codes held in more than seed_max_occurrences places are no seeds
    // either (with self set, other_index already counts them).
    const auto own_index = self ? SeedIndex() : index_of(a);
    std::vector<SeedIndex::Place> found;
    for (std::uint32_t sequence = 0; sequence < a.size(); ++sequence) {
        const auto codes = a[sequence].codes;
        for (std::int32_t position = 0; position < static_cast<std::int32_t>(codes.size()); ++position) {
            const auto code = codes[static_cast<std::size_t>(position)];
            other_index.near(code, rules.seed_bits, seed_max_occurrences, found);
            if (found.empty()) continue;
            if (!self && own_index.count(code, rules.seed_bits, seed_max_occurrences) > seed_max_occurrences) continue;
            for (const auto& at : found) {
                if (self) {
                    // Each pair once, from its earlier copy.
                    if (std::tie(at.sequence, at.position) <= std::tie(sequence, position)) continue;
                    if (at.sequence == sequence) {
                        if (at.position - position < min_gap) continue;
                    } else if ((a[at.sequence].start + at.position * code_period) - (a[sequence].start + position * code_period) <
                               min_self_distance) {
                        continue;
                    }
                }
                seeds[{sequence, at.sequence, at.position - position}].push_back(position);
            }
        }
    }

    return extend_seeds(seeds, a, other, self, rules);
}

std::vector<std::vector<Match>> find_matches_in_groups(std::span<const Sequence> a,
                                                       std::span<const std::vector<Sequence>> groups,
                                                       const MatchRules& rules) {
    // One index over every group's sequences; group_of maps an index sequence
    // to its group, first_of a group to its first index sequence.
    std::vector<Sequence> all;
    std::vector<std::uint32_t> group_of, first_of;
    for (std::uint32_t group = 0; group < groups.size(); ++group) {
        first_of.push_back(static_cast<std::uint32_t>(all.size()));
        for (const auto& sequence : groups[group]) {
            all.push_back(sequence);
            group_of.push_back(group);
        }
    }
    const auto other_index = index_of(all);
    const auto own_index = index_of(a);
    std::vector<Seeds> seeds(groups.size());
    std::vector<std::uint32_t> places;
    std::vector<SeedIndex::Place> found;
    for (std::uint32_t sequence = 0; sequence < a.size(); ++sequence) {
        const auto codes = a[sequence].codes;
        for (std::int32_t position = 0; position < static_cast<std::int32_t>(codes.size()); ++position) {
            const auto code = codes[static_cast<std::size_t>(position)];
            other_index.all_near(code, rules.seed_bits, places);
            if (places.empty()) continue;
            if (own_index.count(code, rules.seed_bits, seed_max_occurrences) > seed_max_occurrences) continue;
            std::ranges::sort(places);
            // Each group's places in turn; a group holding the code in more than
            // seed_max_occurrences places gives no seeds, as one call per group.
            for (std::size_t at = 0; at < places.size();) {
                const auto group = group_of[other_index.sequence_of(places[at])];
                found.clear();
                for (; at < places.size(); ++at) {
                    const auto in = other_index.sequence_of(places[at]);
                    if (group_of[in] != group) break;
                    found.push_back({in - first_of[group], static_cast<std::int32_t>(places[at] - other_index.start_of(in))});
                }
                if (found.size() > seed_max_occurrences) continue;
                for (const auto& place : found)
                    seeds[group][{sequence, place.sequence, place.position - position}].push_back(position);
            }
        }
    }
    std::vector<std::vector<Match>> matches(groups.size());
    for (std::size_t group = 0; group < groups.size(); ++group)
        if (!seeds[group].empty()) matches[group] = extend_seeds(seeds[group], a, groups[group], false, rules);
    return matches;
}
}
