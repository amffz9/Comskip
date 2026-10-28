#include "fingerprint/live_matcher.h"
#include "fingerprint/matching.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace comskip::fingerprint {
namespace {
// A code value held in more places than a seed may be; it is never a seed again.
constexpr std::uint32_t saturated = 0xffffffffu;
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
    codes_.insert(codes_.end(), codes.begin(), codes.end());
    starts_.push_back(static_cast<std::uint32_t>(codes_.size()));
    ids_.push_back(id);
    statuses_.push_back(status);
}

void LiveIndex::finish() {
    order_.resize(codes_.size());
    for (std::uint32_t place = 0; place < order_.size(); ++place) order_[place] = place;
    std::ranges::sort(order_, [&](std::uint32_t x, std::uint32_t y) { return codes_[x] < codes_[y] || (codes_[x] == codes_[y] && x < y); });
    codes_.shrink_to_fit();
}

std::span<const std::uint32_t> LiveIndex::codes(std::size_t entry) const {
    return std::span(codes_).subspan(starts_[entry], starts_[entry + 1] - starts_[entry]);
}

std::size_t LiveIndex::bytes() const {
    return (codes_.capacity() + order_.capacity() + starts_.capacity()) * sizeof(std::uint32_t) +
           ids_.capacity() * sizeof(long long) + statuses_.capacity() * sizeof(Status);
}

void LiveIndex::hits(std::uint32_t code, std::vector<std::pair<std::size_t, long>>& found) const {
    found.clear();
    const auto [first, last] = std::ranges::equal_range(order_, code, {}, [&](std::uint32_t place) { return codes_[place]; });
    if (static_cast<std::size_t>(last - first) > seed_max_occurrences) return;
    for (auto at = first; at != last; ++at) {
        const auto entry = static_cast<std::size_t>(std::ranges::upper_bound(starts_, *at) - starts_.begin() - 1);
        found.emplace_back(entry, static_cast<long>(*at - starts_[entry]));
    }
}

std::size_t LiveMatcher::DiagonalHash::operator()(const DiagonalKey& key) const {
    auto value = static_cast<std::uint64_t>(key.source) * 0x9e3779b97f4a7c15ull;
    value ^= static_cast<std::uint64_t>(static_cast<long long>(key.offset)) + 0x632be59bd9b4e019ull + (value << 6) + (value >> 2);
    return static_cast<std::size_t>(value);
}

LiveMatcher::LiveMatcher() = default;
LiveMatcher::~LiveMatcher() = default;

void LiveMatcher::refresh(std::shared_ptr<const LiveIndex> index) { index_ = std::move(index); }

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
        index_->hits(code, hits_);
        for (const auto& [entry, at] : hits_) seed({index_->id(entry), at - position}, position, index_->status(entry), index_->codes(entry));
    }
    // The recording's own earlier codes, at least 20 s before.
    auto& places = own_[code];
    if (places.size() != 1 || places[0] != saturated) {
        const double time = run.start + position * code_period;
        for (const auto place : places) {
            const auto other = static_cast<std::size_t>(
                std::ranges::upper_bound(runs_, place, {}, &OwnRun::first) - runs_.begin() - 1);
            const long at = static_cast<long>(place - runs_[other].first);
            if (other == current ? position - at < self_gap()
                                 : time - (runs_[other].start + at * code_period) < min_self_distance)
                continue;
            seed({-static_cast<long long>(other) - 1, at - position}, position, Status::candidate, {});
        }
        if (places.size() >= seed_max_occurrences) {
            places.assign(1, saturated);
            places.shrink_to_fit();
        } else {
            places.push_back(run.first + static_cast<std::uint32_t>(position));
        }
    }

    for (auto& track : tracks_)
        if (!track.done) changed_ = advance(track, position) || changed_;
    std::erase_if(tracks_, [](const Track& track) { return track.done && track.match == no_match; });
    if (position % prune_every == 0)
        std::erase_if(diagonals_, [&](const auto& item) { return position - item.second.last > seed_group_gap; });
}

void LiveMatcher::seed(const DiagonalKey& key, long position, Status status, std::span<const std::uint32_t> entry_codes) {
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
    for (const auto& track : tracks_) {
        if (track.source != key.source || std::abs(track.offset - key.offset) > neighbour_offsets) continue;
        if (!track.done || diagonal.first < track.end + seed_group_gap) return;
    }
    Track track;
    track.source = key.source;
    track.offset = key.offset;
    track.status = status;
    if (key.source > 0) track.entry_codes = std::make_shared<const std::vector<std::uint32_t>>(entry_codes.begin(), entry_codes.end());
    else track.own_run = static_cast<std::size_t>(-key.source - 1);
    track.seed = track.next = diagonal.first;
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
        if (track.match == no_match && track.end - track.begin >= min_codes()) {
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
