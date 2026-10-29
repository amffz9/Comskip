#include "fingerprint/live_matcher.h"
#include "fingerprint/matching.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <iterator>
#include <stdexcept>

namespace comskip::fingerprint {
namespace {
// Seeds on the same source this close to a followed diagonal belong to it.
constexpr long neighbour_offsets = 3;
// Diagonals whose seeds are older than this are forgotten every so many codes.
constexpr long prune_every = 64;
// Live, one seed starts following a diagonal (the offline search wants two
// within seed_group_gap): exact seeds can be sparse at an ad's start, and a
// second one came up to 3 s late on g5, past the 6 s marking budget. The 3 s
// of agreement a match needs still decides.
constexpr int live_seeds_needed = 1;

long min_codes() { return codes_for(min_repeat_seconds); }
long self_gap() { return static_cast<long>(min_self_distance / code_period); }
}

void LiveIndex::add(long long id, Status status, std::span<const std::uint32_t> codes) {
    codes_.add(codes);
    ids_.push_back(id);
    statuses_.push_back(status);
}

void LiveIndex::finish() { codes_.finish(); }

std::span<const std::uint32_t> LiveIndex::codes(std::size_t entry) const { return codes_.codes(entry); }

std::size_t LiveIndex::bytes() const {
    return codes_.bytes() + ids_.capacity() * sizeof(long long) + statuses_.capacity() * sizeof(Status);
}

void LiveIndex::hits(std::uint32_t code, int bits, std::vector<SeedIndex::Place>& found) const {
    codes_.near(code, bits, seed_max_occurrences, found);
}

std::size_t LiveMatcher::DiagonalHash::operator()(const DiagonalKey& key) const {
    auto value = static_cast<std::uint64_t>(key.source) * 0x9e3779b97f4a7c15ull;
    value ^= static_cast<std::uint64_t>(static_cast<long long>(key.offset)) + 0x632be59bd9b4e019ull + (value << 6) + (value >> 2);
    return static_cast<std::size_t>(value);
}

void OwnPlaces::places(std::uint32_t code, std::vector<std::uint32_t>& found) const {
    found.clear();
    if (saturated(code)) return;
    // A code saturated since the last merge still has pairs here.
    const auto take = [&](const Pair& pair) {
        if (std::popcount(pair.code ^ code) <= bits_ && (pair.code == code || !saturated(pair.code))) found.push_back(pair.place);
    };
    // Within `bits`, one half of the code is within bits / 2 (as in SeedIndex).
    const int half = bits_ / 2;
    const auto high_near = [&](const Pair& pair) { return std::popcount((pair.code ^ code) >> 16) <= half; };
    const auto visit_keys = [&](std::uint32_t key, auto&& each) {
        // Keys within `half` bits: half is at most 2 for the seed tolerances used.
        each(key);
        for (int bit = 0; bit < 16 && half >= 1; ++bit) {
            each(key ^ (1u << bit));
            for (int other = bit + 1; other < 16 && half >= 2; ++other) each(key ^ (1u << bit) ^ (1u << other));
        }
    };
    visit_keys(code >> 16, [&](std::uint32_t key) {
        const auto first = std::ranges::lower_bound(sorted_, key << 16, {}, &Pair::code);
        for (auto at = first; at != sorted_.end() && at->code >> 16 == key; ++at) take(*at);
    });
    visit_keys(code & 0xffff, [&](std::uint32_t key) {
        const auto [first, last] = std::ranges::equal_range(low_, key, {}, [&](std::uint32_t at) { return sorted_[at].code & 0xffff; });
        for (auto at = first; at != last; ++at)
            if (!high_near(sorted_[*at])) take(sorted_[*at]);
    });
    for (const auto& pair : recent_) take(pair);
    std::ranges::sort(found);
}

bool OwnPlaces::saturated(std::uint32_t code) const { return std::ranges::binary_search(saturated_, code); }

void OwnPlaces::add(std::uint32_t code, std::uint32_t place, std::size_t held) {
    if (held >= seed_max_occurrences) {
        saturated_.insert(std::ranges::upper_bound(saturated_, code), code);
        return;
    }
    recent_.push_back({code, place});
    if (recent_.size() >= recent_limit) merge();
}

// Places only grow, so sorting by (code, place) keeps each code's places in the
// order added. Saturated codes' places are dropped here.
void OwnPlaces::merge() {
    std::ranges::sort(recent_);
    std::vector<Pair> merged;
    merged.reserve(sorted_.size() + recent_.size());
    std::ranges::merge(sorted_, recent_, std::back_inserter(merged));
    std::erase_if(merged, [&](const Pair& pair) { return saturated(pair.code); });
    merged.shrink_to_fit();
    sorted_ = std::move(merged);
    recent_.clear();
    if (bits_ == 0) return;  // exact codes are found through sorted_ alone
    low_.resize(sorted_.size());
    for (std::uint32_t at = 0; at < low_.size(); ++at) low_[at] = at;
    std::ranges::stable_sort(low_, {}, [&](std::uint32_t at) { return sorted_[at].code & 0xffff; });
    low_.shrink_to_fit();
}

std::size_t OwnPlaces::bytes() const {
    return (sorted_.capacity() + recent_.capacity()) * sizeof(Pair) + (saturated_.capacity() + low_.capacity()) * sizeof(std::uint32_t);
}

LiveMatcher::LiveMatcher(MatchRules rules) : rules_(rules), own_(rules.seed_bits) {}
LiveMatcher::~LiveMatcher() = default;

void LiveMatcher::refresh(std::shared_ptr<const LiveIndex> index) { index_ = std::move(index); }

std::size_t LiveMatcher::own_bytes() const {
    std::size_t bytes = own_.bytes();
    for (const auto& run : runs_) bytes += run.codes.capacity() * sizeof(std::uint32_t);
    return bytes;
}

long long LiveMatcher::codes() const {
    long long count = 0;
    for (const auto& run : runs_) count += static_cast<long long>(run.codes.size());
    return count;
}

void LiveMatcher::run_start(double start) {
    finish();
    const auto first = static_cast<std::uint32_t>(codes());
    runs_.push_back({start, first, {}});
}

void LiveMatcher::finish() {
    if (runs_.empty()) return;
    const long last = static_cast<long>(runs_.back().codes.size()) - 1;
    for (auto& track : tracks_)
        if (!track.done) changed_ = end_track(track, completed(last)) || changed_;
    tracks_.clear();
    diagonals_.clear();
}

bool LiveMatcher::feed(std::span<const std::uint32_t> codes) {
    if (runs_.empty()) throw std::logic_error("LiveMatcher::feed before run_start");
    changed_ = false;
    for (const auto code : codes) add_code(code);
    return changed_;
}

double LiveMatcher::completed(long position) const {
    return runs_.back().start + (static_cast<double>(position) * code_samples + code_span_samples) / sample_rate;
}

std::span<const std::uint32_t> LiveMatcher::reference(const Track& track) const {
    if (track.entry_codes) return *track.entry_codes;
    return runs_[track.own_run].codes;
}

bool LiveMatcher::smooth(const Track& track, long position) const {
    const auto& codes = runs_.back().codes;
    const auto other = reference(track);
    int count = 0;
    for (long k = position - smooth_before; k < position + smooth_after; ++k) {
        const long j = k + track.offset;
        if (k >= 0 && j >= 0 && k < static_cast<long>(codes.size()) && j < static_cast<long>(other.size()) &&
            codes_agree(codes[static_cast<std::size_t>(k)], other[static_cast<std::size_t>(j)]))
            ++count;
    }
    return count >= smooth_needed;
}

void LiveMatcher::add_code(std::uint32_t code) {
    auto& run = runs_.back();
    const long position = static_cast<long>(run.codes.size());
    run.codes.push_back(code);
    const std::size_t current = runs_.size() - 1;

    if (index_) {
        index_->hits(code, rules_.seed_bits, hits_);
        for (const auto& [entry, at] : hits_) {
            const auto codes = index_->codes(entry);
            seed({index_->id(entry), static_cast<long>(at) - position}, position, index_->status(entry), codes,
                 codes[static_cast<std::size_t>(at)] == code);
        }
    }
    // The recording's own earlier codes, at least 20 s before.
    if (!own_.saturated(code)) {
        own_.places(code, places_);
        const double time = run.start + position * code_period;
        for (const auto place : places_) {
            const auto other = static_cast<std::size_t>(
                std::ranges::upper_bound(runs_, place, {}, &OwnRun::first) - runs_.begin() - 1);
            const long at = static_cast<long>(place - runs_[other].first);
            if (other == current ? position - at < self_gap()
                                 : time - (runs_[other].start + at * code_period) < min_self_distance)
                continue;
            seed({-static_cast<long long>(other) - 1, at - position}, position, Status::candidate, {},
                 runs_[other].codes[static_cast<std::size_t>(at)] == code);
        }
        own_.add(code, run.first + static_cast<std::uint32_t>(position), places_.size());
    }

    for (auto& track : tracks_)
        if (!track.done) changed_ = advance(track, position) || changed_;
    std::erase_if(tracks_, [](const Track& track) { return track.done && track.match == no_match; });
    if (position % prune_every == 0)
        std::erase_if(diagonals_, [&](const auto& item) { return position - item.second.last > seed_group_gap; });
}

void LiveMatcher::seed(const DiagonalKey& key, long position, Status status, std::span<const std::uint32_t> entry_codes, bool exact) {
    auto& diagonal = diagonals_[key];
    if (diagonal.count > 0 && position - diagonal.last <= seed_group_gap) {
        ++diagonal.count;
    } else {
        diagonal.count = 1;
        diagonal.first = position;
    }
    diagonal.last = position;
    if (diagonal.count < live_seeds_needed) return;
    // A track already follows this diagonal or one beside it, or did across here.
    // Near seeds also fall on the offsets beside a repeat's own (neighbouring
    // codes differ by few bits), so an exact seed moves a track that near
    // seeds alone started, and that has reported no match yet, to its offset.
    for (auto& track : tracks_) {
        if (track.source != key.source || std::abs(track.offset - key.offset) > neighbour_offsets) continue;
        if (exact && !track.exact && !track.done && track.match == no_match && track.offset != key.offset) {
            track.offset = key.offset;
            track.exact = true;
            track.begin = track.end = -1;
            track.seed = track.next = std::min(track.seed, diagonal.first);
            return;
        }
        if (!track.done || diagonal.first < track.end + seed_group_gap) return;
    }
    Track track;
    track.source = key.source;
    track.offset = key.offset;
    track.status = status;
    if (key.source > 0) track.entry_codes = std::make_shared<const std::vector<std::uint32_t>>(entry_codes.begin(), entry_codes.end());
    else track.own_run = static_cast<std::size_t>(-key.source - 1);
    track.seed = track.next = diagonal.first;
    track.exact = exact;
    tracks_.push_back(std::move(track));
}

bool LiveMatcher::advance(Track& track, long newest) {
    bool changed = false;
    const long last = newest - (smooth_after - 1);  // the window of a later position is not complete yet
    const long size = static_cast<long>(reference(track).size());
    for (; !track.done && track.next <= last; ++track.next) {
        const long position = track.next;
        if (track.begin < 0) {
            if (smooth(track, position)) {
                track.begin = position;
                while (smooth(track, track.begin - 1)) --track.begin;
                track.end = position + 1;
            } else if (position - track.seed > 2 * seed_group_gap) {
                track.done = true;  // the seeds never grew into agreement
                continue;
            } else {
                continue;
            }
        } else if (smooth(track, position)) {
            track.end = std::max(track.end, position + 1);
            if (track.match != no_match) matches_[track.match].stopped_at = -1;
        } else {
            if (position == track.end && track.match != no_match) matches_[track.match].stopped_at = completed(newest);
            if (position - track.end + 1 >= divergence_codes) {
                changed = end_track(track, completed(newest)) || changed;
                break;
            }
            continue;
        }
        // The end of the reference copy, or a self-repeat's first copy reaching
        // where the second begins.
        const long limit = track.entry_codes || track.own_run != runs_.size() - 1 ? size - track.offset : track.begin - track.offset;
        const long slack = track.entry_codes || track.own_run != runs_.size() - 1 ? edge_slack : 0;
        if (track.end >= limit - slack) {
            track.end = limit;
            track.complete = true;
        }
        if (track.match == no_match && track.end - track.begin >= min_codes() && good_enough(track)) {
            track.match = matches_.size();
            matches_.push_back({track.source > 0 ? track.source : 0, track.status});
        }
        if (track.complete) {
            changed = end_track(track, completed(newest)) || changed;
        } else if (track.match != no_match) {
            const auto before = matches_[track.match];
            update(track);
            const auto& after = matches_[track.match];
            changed = changed || before.agreed_from != after.agreed_from || before.agreed_to != after.agreed_to ||
                      before.from != after.from || before.to != after.to;
        }
    }
    return changed;
}

// The quality gate over the agreed part so far, as offline: a stretch that
// fails it may still pass as it grows. Live, one exact seed started a match
// before near seeds (live_seeds_needed), so one equal code stands in for the
// offline exact pair.
bool LiveMatcher::good_enough(const Track& track) const {
    if (!rules_.quality_gate) return true;
    auto quality = quality_of(runs_.back().codes, reference(track), track.offset, track.begin, track.end);
    quality.exact_pair = quality.exact_pair || (live_seeds_needed == 1 && quality.exact_code);
    return passes_gate(quality, static_cast<double>(track.end - track.begin) * code_period);
}

bool LiveMatcher::end_track(Track& track, double now) {
    track.done = true;
    if (track.match == no_match) return false;
    update(track);
    auto& match = matches_[track.match];
    match.ended_at = now;
    if (match.stopped_at < 0 && !track.complete) match.stopped_at = now;
    return true;
}

void LiveMatcher::update(Track& track) {
    auto& match = matches_[track.match];
    const auto& run = runs_.back();
    const auto time = [&](double start, long position) { return start + position * code_period; };
    match.agreed_from = time(run.start, track.begin) + edge_start;
    match.agreed_to = time(run.start, track.end) + edge_end;
    match.ended = track.done;
    if (track.entry_codes) {
        // The extent: the entry's codes placed by the offset. Codes before the
        // agreed part that this run holds did not agree, so only an entry that
        // began before the run is projected back, to the run's start.
        const long entry_start = -track.offset;
        const long entry_end = static_cast<long>(track.entry_codes->size()) - track.offset;
        match.withdrawn = track.done && !track.complete;
        if (match.withdrawn) {
            match.from = match.agreed_from;
            match.to = match.agreed_to;
        } else {
            match.from = entry_start < 0 && track.begin <= edge_slack ? run.start : match.agreed_from;
            match.to = std::max(match.agreed_to, time(run.start, entry_end) + edge_end);
        }
    } else {
        match.from = match.agreed_from;
        match.to = match.agreed_to;
        const double earlier = runs_[track.own_run].start;
        match.earlier_from = time(earlier, track.begin + track.offset) + edge_start;
        match.earlier_to = time(earlier, track.end + track.offset) + edge_end;
    }
}
}
