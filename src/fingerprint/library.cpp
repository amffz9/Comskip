#include "fingerprint/library.h"
#include "fingerprint/matching.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <tuple>

namespace comskip::fingerprint {
namespace {
constexpr std::array<std::string_view, 6> status_names{"candidate", "ad", "in-programme ad", "intro", "credits", "programme"};
constexpr std::array<double, 8> standard_lengths{10, 15, 20, 30, 45, 60, 90, 120};
constexpr std::array<double, 6> audio_lengths{15, 30, 45, 60, 90, 120};
// "Near the start": begins within the first 20 min of its programme window.
constexpr double near_start_seconds = 20 * 60;
// "Near the end": ends within the last 8 min of the window, or within 2 min after.
constexpr double near_end_before = 8 * 60;
constexpr double near_end_after = 2 * 60;

struct RecordingData {
    Context context;
    std::optional<Evidence> evidence;
    // One series key and one channel for every recording holding the same
    // broadcast as this one; empty when it shares none.
    std::string broadcast, broadcast_channel, broadcast_network;
};

// Longer repeats first, so they define entries; then by position, for a fixed order.
bool longest_first(const Match& x, const Match& y) {
    return std::make_tuple(-x.length(), x.a, x.a_begin, x.b, x.b_begin) < std::make_tuple(-y.length(), y.a, y.a_begin, y.b, y.b_begin);
}

double overlap(double from, double to, double other_from, double other_to) {
    return std::max(0.0, std::min(to, other_to) - std::max(from, other_from));
}

bool seam_near(const Evidence& evidence, double time) {
    const auto found = std::ranges::lower_bound(evidence.seams, time - seam_reach, {}, &Seam::at);
    return found != evidence.seams.end() && found->at <= time + seam_reach;
}

// Seams and logo of an airing, from its own recording's evidence.
void apply_evidence(Airing& airing, const RecordingData& recording) {
    airing.seam_start.reset();
    airing.seam_end.reset();
    airing.logo_fraction.reset();
    airing.logo = "unknown";
    if (!recording.evidence) return;
    const auto& evidence = *recording.evidence;
    const double start = airing.start();
    const double end = airing.end();
    airing.seam_start = seam_near(evidence, start);
    airing.seam_end = seam_near(evidence, end);
    // A recording whose Comskip run learned no logo can't show that an airing
    // has none, so its logo stays unknown.
    if (!evidence.logo_known || end <= start) return;
    double seen = 0;
    for (const auto& interval : evidence.logo) seen += overlap(start, end, interval.from, interval.to);
    airing.logo_fraction = seen / (end - start);
    airing.logo = *airing.logo_fraction < logo_none_below ? "no" : "yes";
}

// A network compared as its affiliation: "CBS" and " cbs" are one.
std::string network_key(std::string_view network) {
    const auto first = network.find_first_not_of(" 	");
    if (first == std::string_view::npos) return "";
    network = network.substr(first, network.find_last_not_of(" 	") - first + 1);
    std::string key;
    for (const char character : network) key += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return key;
}

// Two airings went out on different stations: by network when both know one,
// by channel otherwise. Nothing unknown counts as different.
bool different_places(const AiringFacts& one, const AiringFacts& other) {
    if (!one.network.empty() && !other.network.empty()) return one.network != other.network;
    return !one.channel.empty() && !other.channel.empty() && one.channel != other.channel;
}

AiringFacts facts_of(const Airing& airing, const RecordingData& recording) {
    AiringFacts facts;
    facts.recording = airing.recording;
    // Two recordings of one broadcast name its channel differently ("CBS",
    // "4.1"), so they count as one channel.
    facts.channel = recording.broadcast.empty() ? recording.context.channel : recording.broadcast_channel;
    facts.network = recording.broadcast.empty() ? network_key(recording.context.network) : recording.broadcast_network;
    const double start = airing.start();
    const double end = airing.end();
    const Programme* programme = programme_at(recording.context, start);
    if (!programme) programme = programme_at(recording.context, std::max(0.0, start - near_end_after));
    if (programme) {
        facts.series_key = programme->series_key;
        facts.near_start = start - programme->from <= near_start_seconds;
        facts.near_end = end >= programme->to - near_end_before && end <= programme->to + near_end_after;
        facts.has_position = true;
        facts.from_start = start - programme->from;
        facts.to_end = programme->to - end;
    }
    facts.broadcast = recording.broadcast.empty() ? facts.series_key : recording.broadcast;
    facts.has_evidence = airing.logo != "unknown";
    facts.seam_start = airing.seam_start.value_or(false);
    facts.seam_end = airing.seam_end.value_or(false);
    facts.logo_fraction = airing.logo_fraction.value_or(0);
    return facts;
}

// A Plex file named for its air date and no season or episode (" - 2026-09-13
// 07 00 00 - Chicago Bears at Carolina Panthers.ts") holds one event, a game:
// every such file of a series is its own broadcast, since games share ads but
// not programme.
bool dated_event(const std::string& recording, const RecordingData& data) {
    static const std::regex dated(R"( - \d{4}-\d{2}-\d{2} )");
    static const std::regex episode(R"(S\d+E\d+)");
    return data.context.source == "plex" && std::regex_search(recording, dated) && !std::regex_search(recording, episode);
}

std::string join(const std::set<std::string>& values) {
    std::string text;
    for (const auto& value : values) {
        if (!text.empty()) text += ',';
        text += value;
    }
    return text;
}

// The working copy of the store for one search: every entry and airing, the
// contexts and evidence of every recording.
class Search {
public:
    Search(Store& store, const LibraryOptions& options, std::string recording)
        : store_(store), options_(options), recording_(std::move(recording)) {
        load();
    }

    RepeatsResult run(const Evidence* evidence) {
        if (!recordings_.contains(recording_)) throw std::invalid_argument("Recording " + recording_ + " has no stored stream");
        if (evidence) store_evidence(*evidence);
        const auto own = runs_of(recording_);
        std::vector<Sequence> own_sequences;
        for (const auto& run : own) own_sequences.push_back({run.codes, run.start});

        match_entries(own, own_sequences);
        auto self_matches = find_matches(own_sequences, {}, true, options_.matching);
        std::ranges::sort(self_matches, longest_first);
        for (const auto& match : self_matches)
            add_pair(recording_, own[match.a], recording_, own[match.b], match);
        for (const auto& [other, data] : recordings_) {
            if (other == recording_) continue;
            const auto theirs = runs_of(other);
            std::vector<Sequence> their_sequences;
            for (const auto& run : theirs) their_sequences.push_back({run.codes, run.start});
            auto matches = find_matches(own_sequences, their_sequences, false, options_.matching);
            std::ranges::sort(matches, longest_first);
            for (const auto& match : matches) add_pair(recording_, own[match.a], other, theirs[match.b], match);
        }
        split_entries();
        // The recording's evidence may have changed, so all its airings are
        // looked at again.
        for (std::size_t index = 0; index < airings_.size(); ++index)
            if (alive_[index] && airings_[index].recording == recording_) touched_.insert(airings_[index].entry);
        return save();
    }

    // The rules again over every entry, with nothing searched.
    RepeatsResult promote_all() { return save(); }

private:
    void load() {
        auto recordings = store_.prepare("SELECT id, context, evidence FROM recording ORDER BY id");
        while (recordings.step()) {
            RecordingData data;
            data.context = context_from_json(json::parse(recordings.text(1)));
            if (!recordings.is_null(2)) data.evidence = evidence_from_json(json::parse(recordings.text(2)));
            recordings_.emplace(recordings.text(0), std::move(data));
        }
        auto entries = store_.prepare("SELECT id, status, length, codes, reason FROM entry ORDER BY id");
        while (entries.step()) {
            EntryInfo entry;
            entry.id = entries.integer(0);
            entry.status = status_from_name(entries.text(1));
            entry.length = entries.real(2);
            entry.codes = entries.codes(3);
            entry.reason = entries.text(4);
            old_status_[entry.id] = entry.status;
            old_reason_[entry.id] = entry.reason;
            // Before the audio-only rules, the intro and credits rule made
            // entries programme until step T4b, final; they are looked at again.
            if (entry.status == Status::programme && entry.reason.find("rule off until T4b") != std::string::npos) {
                entry.status = Status::candidate;
                migrated_.insert(entry.id);
            }
            entries_.emplace(entry.id, std::move(entry));
        }
        auto airings = store_.prepare(
            "SELECT entry, recording, run_start, code_begin, code_end, part_begin, part_end, cut_at_start, cut_at_end, "
            "found_by, seam_start, seam_end, logo, logo_fraction FROM airing ORDER BY id");
        while (airings.step()) {
            Airing airing;
            airing.entry = airings.integer(0);
            airing.recording = airings.text(1);
            airing.run_start = airings.real(2);
            airing.code_begin = static_cast<long>(airings.integer(3));
            airing.code_end = static_cast<long>(airings.integer(4));
            airing.part_begin = static_cast<long>(airings.integer(5));
            airing.part_end = static_cast<long>(airings.integer(6));
            airing.cut_at_start = airings.integer(7) != 0;
            airing.cut_at_end = airings.integer(8) != 0;
            airing.found_by = airings.text(9);
            if (!airings.is_null(10)) airing.seam_start = airings.integer(10) != 0;
            if (!airings.is_null(11)) airing.seam_end = airings.integer(11) != 0;
            airing.logo = airings.text(12);
            if (!airings.is_null(13)) airing.logo_fraction = airings.real(13);
            add_airing(std::move(airing), false);
        }
    }

    std::vector<CodeRun> runs_of(const std::string& recording) {
        std::vector<CodeRun> runs;
        auto query = store_.prepare("SELECT start, codes FROM run WHERE recording = ? ORDER BY start");
        query.bind(1, recording);
        while (query.step()) runs.push_back({query.real(0), query.codes(1)});
        return runs;
    }

    void store_evidence(const Evidence& evidence) {
        in_transaction(store_, [&] {
            store_.prepare("UPDATE recording SET evidence = ? WHERE id = ?").bind(1, evidence_to_json(evidence)).bind(2, recording_).run();
        });
        recordings_[recording_].evidence = evidence;
    }

    std::size_t add_airing(Airing airing, bool fresh) {
        if (fresh) {
            apply_evidence(airing, recordings_[airing.recording]);
            touched_.insert(airing.entry);
            fresh_in_[airing.entry].insert(airing.recording);
            ++added_;
        }
        by_recording_[airing.recording].push_back(airings_.size());
        by_entry_[airing.entry].push_back(airings_.size());
        airings_.push_back(std::move(airing));
        alive_.push_back(true);
        return airings_.size() - 1;
    }

    // The live airing of the recording whose codes hold code `at` of the run.
    const Airing* covering(const std::string& recording, double run_start, long at) const {
        const auto found = by_recording_.find(recording);
        if (found == by_recording_.end()) return nullptr;
        for (const auto index : found->second) {
            const auto& airing = airings_[index];
            if (alive_[index] && airing.run_start == run_start && airing.code_begin <= at && at < airing.code_end) return &airing;
        }
        return nullptr;
    }
    // Where airings of the recording begin or end inside codes from..to of the run.
    void edges_inside(const std::string& recording, double run_start, long from, long to, long shift, std::vector<long>& edges) const {
        const auto found = by_recording_.find(recording);
        if (found == by_recording_.end()) return;
        for (const auto index : found->second) {
            const auto& airing = airings_[index];
            if (!alive_[index] || airing.run_start != run_start) continue;
            for (const long edge : {airing.code_begin, airing.code_end})
                if (from < edge && edge < to) edges.push_back(edge - shift);
        }
    }

    // A stretch of this recording agrees with an entry's reference copy: each
    // part not already an airing here becomes an airing of that entry.
    void match_entries(const std::vector<CodeRun>& own, const std::vector<Sequence>& own_sequences) {
        std::vector<long long> ids;
        std::vector<Sequence> references;
        for (const auto& [id, entry] : entries_) {
            ids.push_back(id);
            references.push_back({entry.codes, 0});
        }
        if (references.empty()) return;
        auto matches = find_matches(own_sequences, references, false, options_.matching);
        std::ranges::sort(matches, longest_first);
        for (const auto& match : matches) {
            const auto& run = own[match.a];
            std::vector<long> cuts{0, match.length()};
            edges_inside(recording_, run.start, match.a_begin, match.a_end, match.a_begin, cuts);
            std::ranges::sort(cuts);
            for (std::size_t piece = 0; piece + 1 < cuts.size(); ++piece) {
                const long from = cuts[piece];
                const long to = cuts[piece + 1];
                if (to - from < codes_for(min_repeat_seconds)) continue;
                if (covering(recording_, run.start, match.a_begin + (from + to) / 2)) continue;
                Airing airing;
                airing.entry = ids[match.b];
                airing.recording = recording_;
                airing.run_start = run.start;
                airing.code_begin = match.a_begin + from;
                airing.code_end = match.a_begin + to;
                airing.part_begin = match.b_begin + from;
                airing.part_end = match.b_begin + to;
                airing.cut_at_start = from > 0 || match.a_cut_at_start;
                airing.cut_at_end = to < match.length() || match.a_cut_at_end;
                airing.found_by = options_.found_by;
                add_airing(std::move(airing), true);
            }
        }
    }

    // A repeat between two stretches (in this recording, or this one and a
    // stored one). Cut at the edges of airings already on either side; a piece
    // known on one side becomes an airing of that entry on the other, and a
    // piece known on neither becomes a new candidate entry with both airings.
    void add_pair(const std::string& first, const CodeRun& first_run, const std::string& second, const CodeRun& second_run,
                  const Match& match) {
        const long length = match.length();
        std::vector<long> cuts{0, length};
        edges_inside(first, first_run.start, match.a_begin, match.a_end, match.a_begin, cuts);
        edges_inside(second, second_run.start, match.b_begin, match.b_end(), match.b_begin, cuts);
        std::ranges::sort(cuts);
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
        for (std::size_t piece = 0; piece + 1 < cuts.size(); ++piece) {
            const long from = cuts[piece];
            const long to = cuts[piece + 1];
            if (to - from < codes_for(min_repeat_seconds)) continue;
            const long middle = (from + to) / 2;
            const Airing* known_first = covering(first, first_run.start, match.a_begin + middle);
            const Airing* known_second = covering(second, second_run.start, match.b_begin + middle);
            if (known_first && known_second) continue;
            const auto make = [&](const std::string& recording, const CodeRun& run, long begin, bool cut_start, bool cut_end) {
                Airing airing;
                airing.recording = recording;
                airing.run_start = run.start;
                airing.code_begin = begin + from;
                airing.code_end = begin + to;
                airing.cut_at_start = from > 0 || cut_start;
                airing.cut_at_end = to < length || cut_end;
                airing.found_by = options_.found_by;
                return airing;
            };
            auto one = make(first, first_run, match.a_begin, match.a_cut_at_start, match.a_cut_at_end);
            auto two = make(second, second_run, match.b_begin, match.b_cut_at_start, match.b_cut_at_end);
            if (known_first || known_second) {
                const Airing known = known_first ? *known_first : *known_second;
                auto& added = known_first ? two : one;
                const long known_begin = known_first ? one.code_begin : two.code_begin;
                const auto& entry = entries_.at(known.entry);
                added.entry = known.entry;
                added.part_begin = known.part_begin + (known_begin - known.code_begin);
                added.part_end = added.part_begin + (to - from);
                // Keep the airing inside the entry's codes.
                const long low = std::max(0L, -added.part_begin);
                const long high = std::max(0L, added.part_end - static_cast<long>(entry.codes.size()));
                if (to - from - low - high < codes_for(min_repeat_seconds)) continue;
                added.part_begin += low;
                added.code_begin += low;
                added.part_end -= high;
                added.code_end -= high;
                if (low) added.cut_at_start = true;
                if (high) added.cut_at_end = true;
                add_airing(std::move(added), true);
                continue;
            }
            EntryInfo entry;
            entry.id = next_new_--;
            entry.status = Status::candidate;
            entry.reason = "new repeat";
            const auto& source = first_run.codes;
            entry.codes.assign(source.begin() + one.code_begin, source.begin() + one.code_end);
            entry.length = static_cast<double>(entry.codes.size()) * code_period + edge_end - edge_start;
            one.entry = two.entry = entry.id;
            one.part_begin = two.part_begin = 0;
            one.part_end = two.part_end = to - from;
            entries_.emplace(entry.id, std::move(entry));
            add_airing(std::move(one), true);
            add_airing(std::move(two), true);
        }
    }

    // An entry two of whose airings agree on only part of it (an airing that
    // stopped agreeing inside it, not at a run's edge) is split there: the shared
    // part and the rest become entries of their own.
    void split_entries() {
        const long min_codes = codes_for(min_repeat_seconds);
        const std::vector<long long> candidates(touched_.begin(), touched_.end());
        for (const auto id : candidates) {
            auto& entry = entries_.at(id);
            const long size = static_cast<long>(entry.codes.size());
            std::vector<long> points;
            const auto members = members_of(id);
            for (const auto index : members) {
                const auto& airing = airings_[index];
                if (!airing.cut_at_start && airing.part_begin >= min_codes && airing.part_begin <= size - min_codes)
                    points.push_back(airing.part_begin);
                if (!airing.cut_at_end && airing.part_end >= min_codes && airing.part_end <= size - min_codes)
                    points.push_back(airing.part_end);
            }
            std::ranges::sort(points);
            std::vector<long> cuts{0};
            for (const long point : points)
                if (point - cuts.back() >= min_codes) cuts.push_back(point);
            if (size - cuts.back() < min_codes) cuts.pop_back();
            cuts.push_back(size);
            if (cuts.size() <= 2) continue;

            const auto original = entry;
            for (std::size_t piece = 0; piece + 1 < cuts.size(); ++piece) {
                const long from = cuts[piece];
                const long to = cuts[piece + 1];
                long long piece_id = id;
                if (piece > 0) {
                    EntryInfo part;
                    part.id = piece_id = next_new_--;
                    entries_.emplace(part.id, std::move(part));
                }
                auto& target = entries_.at(piece_id);
                target.status = status_is_final(original.status) ? original.status : Status::candidate;
                target.reason = status_is_final(original.status) ? original.reason : "split";
                target.codes.assign(original.codes.begin() + from, original.codes.begin() + to);
                target.length = static_cast<double>(to - from) * code_period + edge_end - edge_start;
                if (piece > 0) old_status_[piece_id] = original.status;
                split_.insert(piece_id);
                touched_.insert(piece_id);
                for (const auto index : members) {
                    const Airing airing = airings_[index];
                    const long begin = std::max(airing.part_begin, from);
                    const long end = std::min(airing.part_end, to);
                    if (end - begin < min_codes) continue;
                    Airing part = airing;
                    part.entry = piece_id;
                    part.code_begin = airing.code_begin + (begin - airing.part_begin);
                    part.code_end = part.code_begin + (end - begin);
                    part.part_begin = begin - from;
                    part.part_end = end - from;
                    part.cut_at_start = begin == airing.part_begin ? airing.cut_at_start : true;
                    part.cut_at_end = end == airing.part_end ? airing.cut_at_end : true;
                    const auto added_before = added_;
                    add_airing(std::move(part), true);
                    added_ = added_before;
                }
            }
            for (const auto index : members) alive_[index] = false;
        }
    }

    // Recordings sharing a repeat of at least same_broadcast_seconds hold one
    // broadcast; each such group counts as one series, its smallest key. A
    // group holding a dated event counts as its own series.
    void link_broadcasts() {
        std::map<std::string, std::string> parent;
        for (const auto& [name, data] : recordings_) parent[name] = name;
        const auto root = [&](std::string name) {
            while (parent[name] != name) name = parent[name] = parent[parent[name]];
            return name;
        };
        for (const auto& [id, entry] : entries_) {
            if (entry.length < same_broadcast_seconds) continue;
            const auto members = members_of(id);
            for (std::size_t n = 1; n < members.size(); ++n) {
                const auto first = root(airings_[members[0]].recording);
                const auto other = root(airings_[members[n]].recording);
                if (first != other) parent[std::max(first, other)] = std::min(first, other);
            }
        }
        std::map<std::string, std::set<std::string>> keys, channels, networks;
        std::map<std::string, int> sizes;
        std::set<std::string> events;
        for (const auto& [name, data] : recordings_) {
            const auto group = root(name);
            ++sizes[group];
            if (dated_event(name, data)) events.insert(group);
            for (const auto& programme : data.context.programmes)
                if (!programme.series_key.empty()) keys[group].insert(programme.series_key);
            if (!data.context.channel.empty()) channels[group].insert(data.context.channel);
            if (const auto network = network_key(data.context.network); !network.empty()) networks[group].insert(network);
        }
        for (auto& [name, data] : recordings_) {
            const auto group = root(name);
            const bool shared = sizes[group] > 1;
            data.broadcast = shared && !keys[group].empty() ? *keys[group].begin() : "";
            if (events.contains(group)) data.broadcast = group;
            data.broadcast_channel = shared && !channels[group].empty() ? *channels[group].begin() : "";
            data.broadcast_network = shared && !networks[group].empty() ? *networks[group].begin() : "";
            broadcast_group_[name] = group;
        }
    }

    // The broadcasts in which an airing of the entry lies next to an airing of
    // an `ad` entry, in the same recording.
    std::set<std::string> beside_ads(long long id, const std::map<long long, Promotion>& promotions) const {
        std::set<std::string> beside;
        for (const auto index : members_of(id)) {
            const auto& airing = airings_[index];
            const double start = airing.start();
            const double end = airing.end();
            for (const auto other : by_recording_.at(airing.recording)) {
                const auto& neighbour = airings_[other];
                if (!alive_[other] || neighbour.entry == id) continue;
                const auto found = promotions.find(neighbour.entry);
                if (found == promotions.end() || found->second.status != Status::ad) continue;
                if (std::abs(start - neighbour.end()) <= adjacent_gap || std::abs(neighbour.start() - end) <= adjacent_gap) {
                    beside.insert(broadcast_group_.at(airing.recording));
                    break;
                }
            }
        }
        return beside;
    }

    // A widely recurring entry that is not an ad's length alone is one when, in
    // some airing, it and the airings it abuts (each of an entry that may join a
    // break) make an ad's length: the Library splits an ad where its variants
    // stop agreeing, so a 30 s spot can be stored as a 22 s body and an 8 s tag.
    // The same chain must recur in chain_recordings recordings, or lie in or
    // next to an ad by its own airings: two recordings sharing a stretch of
    // programme (a game's highlight replayed elsewhere) can split it into
    // pieces that happen to add up to an ad's length.
    void judge_chains(std::map<long long, Promotion>& promotions) const {
        struct Chain {
            std::set<std::string> recordings;
            bool anchored{};
            double length{};
        };
        std::map<std::vector<long long>, Chain> chains;
        const auto own_ad = [&](std::size_t index) { return promotions.at(airings_[index].entry).status == Status::ad; };
        for (const auto& [name, indices] : by_recording_) {
            std::vector<std::size_t> pieces;
            for (const auto index : indices) {
                if (!alive_[index]) continue;
                const auto found = promotions.find(airings_[index].entry);
                if (found != promotions.end() && (found->second.may_join || found->second.widely || found->second.status == Status::ad))
                    pieces.push_back(index);
            }
            std::ranges::sort(pieces, [&](std::size_t x, std::size_t y) { return airings_[x].start() < airings_[y].start(); });
            const auto abuts = [&](std::size_t x, std::size_t y) {
                return std::abs(airings_[pieces[y]].start() - airings_[pieces[x]].end()) <= adjacent_gap;
            };
            for (std::size_t first = 0; first < pieces.size(); ++first) {
                for (std::size_t last = first + 1; last < pieces.size() && last - first < chain_most_pieces; ++last) {
                    if (!abuts(last - 1, last)) break;
                    const double length = airings_[pieces[last]].end() - airings_[pieces[first]].start();
                    if (!audio_ad_length(length)) continue;
                    std::vector<long long> key;
                    bool anchored = (first > 0 && abuts(first - 1, first) && own_ad(pieces[first - 1])) ||
                                    (last + 1 < pieces.size() && abuts(last, last + 1) && own_ad(pieces[last + 1]));
                    for (std::size_t piece = first; piece <= last; ++piece) {
                        key.push_back(airings_[pieces[piece]].entry);
                        anchored = anchored || own_ad(pieces[piece]);
                    }
                    auto& chain = chains[key];
                    chain.recordings.insert(name);
                    chain.anchored = chain.anchored || anchored;
                    chain.length = length;
                }
            }
        }
        for (const auto& [key, chain] : chains) {
            if (static_cast<int>(chain.recordings.size()) < chain_recordings && !chain.anchored) continue;
            for (const auto entry : key) {
                auto& promotion = promotions.at(entry);
                if (promotion.status == Status::ad || !promotion.widely) continue;
                const auto at = promotion.reason.find(", not an ad's length");
                promotion.status = Status::ad;
                promotion.reason = promotion.reason.substr(0, at) + ", no logo seen, " +
                                   std::to_string(static_cast<int>(std::lround(chain.length))) + " s with the airings it abuts";
            }
        }
    }

    // An entry that may join a break and sits next to `ad` airings in at least
    // rules.adjacent_broadcasts broadcasts becomes an ad; repeated until nothing
    // changes, so a break fills outwards from its known ads.
    void join_breaks(std::map<long long, Promotion>& promotions) const {
        const int needed = options_.rules.adjacent_broadcasts;
        if (needed <= 0) return;
        for (bool changed = true; changed;) {
            changed = false;
            std::vector<std::pair<long long, std::size_t>> joined;
            for (const auto& [id, promotion] : promotions) {
                if (promotion.status == Status::ad || !(promotion.may_join || promotion.ad_if_beside)) continue;
                const auto beside = beside_ads(id, promotions);
                if (static_cast<int>(beside.size()) >= needed) joined.emplace_back(id, beside.size());
            }
            for (const auto& [id, count] : joined) {
                auto& promotion = promotions.at(id);
                promotion.reason = promotion.ad_if_beside
                                       ? std::string(status_name(promotion.status)) + " by position, but next to ads in " +
                                             std::to_string(count) + " recordings"
                                       : "next to ads in " + std::to_string(count) + " broadcasts";
                promotion.status = Status::ad;
                promotion.ad_if_beside = false;
                changed = true;
            }
        }
    }

    // Every entry is promoted again, since the rules look across entries and
    // recordings and so an entry this search never touched can change; entries
    // that changed, or that this search touched, are written.
    RepeatsResult save() {
        RepeatsResult result;
        result.airings_found = added_;
        link_broadcasts();
        std::map<long long, Promotion> promotions;
        for (auto& [id, entry] : entries_) {
            std::vector<AiringFacts> facts;
            for (const auto index : members_of(id)) {
                auto& airing = airings_[index];
                auto& recording = recordings_.at(airing.recording);
                if (airing.recording == recording_) apply_evidence(airing, recording);
                facts.push_back(facts_of(airing, recording));
            }
            promotions.emplace(id, promote(entry.status, entry.length, facts, options_.rules));
        }
        judge_chains(promotions);
        join_breaks(promotions);

        std::set<std::string> corrected;
        for (auto& [id, entry] : entries_) {
            const auto& promotion = promotions.at(id);
            const auto before = old_status_.contains(id) ? std::optional(old_status_.at(id)) : std::nullopt;
            entry.status = promotion.status;
            if (!promotion.reason.empty()) entry.reason = promotion.reason;
            const bool changed = (before ? *before != entry.status : entry.status != Status::candidate) || migrated_.contains(id);
            const bool reworded = !old_reason_.contains(id) || old_reason_.at(id) != entry.reason;
            if (!changed && !reworded && !touched_.contains(id)) continue;
            const auto members = members_of(id);
            const bool was_candidate = !before || *before == Status::candidate || migrated_.contains(id);
            if (changed && entry.status != Status::candidate && was_candidate) ++result.promoted;
            const auto correct = [&](const std::string& name) {
                if (name != recording_ && recordings_.at(name).context.source == "capture") corrected.insert(name);
            };
            if (changed || split_.contains(id)) {
                result.changed_entries.push_back(id);
                for (const auto index : members) correct(airings_[index].recording);
            } else if (marked(entry.status) && fresh_in_.contains(id)) {
                // A marked entry found again in another capture changes its marks.
                for (const auto& name : fresh_in_.at(id)) correct(name);
            }
            write_entry(entry, members);
        }
        result.corrected.assign(corrected.begin(), corrected.end());
        // Changed ids refer to rows now in the store.
        for (auto& id : result.changed_entries) id = saved_ids_.contains(id) ? saved_ids_.at(id) : id;
        std::ranges::sort(result.changed_entries);
        return result;
    }

    void write_entry(EntryInfo& entry, const std::vector<std::size_t>& members) {
        double first = 0, last = 0;
        std::set<std::string> series, channels;
        for (std::size_t n = 0; n < members.size(); ++n) {
            const auto& airing = airings_[members[n]];
            const auto& recording = recordings_.at(airing.recording);
            const double when = recording.context.air_start + airing.start();
            first = n ? std::min(first, when) : when;
            last = n ? std::max(last, when) : when;
            if (const auto* programme = programme_at(recording.context, airing.start()); programme && !programme->series_key.empty())
                series.insert(programme->series_key);
            if (!recording.context.channel.empty()) channels.insert(recording.context.channel);
        }
        in_transaction(store_, [&] {
            long long id = entry.id;
            if (id < 0) {
                store_.prepare("INSERT INTO entry(status, length, codes, reason) VALUES('candidate', 0, x'', '')").run();
                id = store_.last_insert_id();
                saved_ids_[entry.id] = id;
            }
            auto update = store_.prepare(
                "UPDATE entry SET status = ?, length = ?, codes = ?, first_airing = ?, last_airing = ?, airings = ?, "
                "series_keys = ?, channels = ?, reason = ? WHERE id = ?");
            update.bind(1, status_name(entry.status)).bind(2, entry.length).bind_codes(3, entry.codes);
            if (members.empty()) update.bind(4, nullptr).bind(5, nullptr);
            else update.bind(4, first).bind(5, last);
            update.bind(6, static_cast<long long>(members.size())).bind(7, join(series)).bind(8, join(channels));
            update.bind(9, entry.reason).bind(10, id).run();
            store_.prepare("DELETE FROM airing WHERE entry = ?").bind(1, id).run();
            auto insert = store_.prepare(
                "INSERT INTO airing(entry, recording, run_start, code_begin, code_end, start, \"end\", part_begin, part_end, "
                "cut_at_start, cut_at_end, found_by, seam_start, seam_end, logo, logo_fraction) "
                "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            for (const auto index : members) {
                auto& airing = airings_[index];
                airing.entry = id;
                insert.bind(1, id).bind(2, airing.recording).bind(3, airing.run_start);
                insert.bind(4, static_cast<long long>(airing.code_begin)).bind(5, static_cast<long long>(airing.code_end));
                insert.bind(6, airing.start()).bind(7, airing.end());
                insert.bind(8, static_cast<long long>(airing.part_begin)).bind(9, static_cast<long long>(airing.part_end));
                insert.bind(10, airing.cut_at_start ? 1 : 0).bind(11, airing.cut_at_end ? 1 : 0).bind(12, airing.found_by);
                if (airing.seam_start) insert.bind(13, *airing.seam_start ? 1 : 0); else insert.bind(13, nullptr);
                if (airing.seam_end) insert.bind(14, *airing.seam_end ? 1 : 0); else insert.bind(14, nullptr);
                insert.bind(15, airing.logo);
                if (airing.logo_fraction) insert.bind(16, *airing.logo_fraction); else insert.bind(16, nullptr);
                insert.run();
            }
        });
    }

    std::vector<std::size_t> members_of(long long id) const {
        std::vector<std::size_t> members;
        if (const auto found = by_entry_.find(id); found != by_entry_.end())
            for (const auto index : found->second)
                if (alive_[index] && airings_[index].entry == id) members.push_back(index);
        return members;
    }
    bool marked(Status status) const {
        return status == Status::ad || status == Status::in_programme_ad ||
               (options_.intro_credits && (status == Status::intro || status == Status::credits));
    }

    Store& store_;
    const LibraryOptions& options_;
    std::string recording_;
    std::map<std::string, RecordingData> recordings_;
    std::map<long long, EntryInfo> entries_;
    std::map<long long, Status> old_status_;
    std::map<long long, std::string> old_reason_;
    std::set<long long> migrated_;  // programme by the old intro and credits rule
    std::map<std::string, std::string> broadcast_group_;
    std::vector<Airing> airings_;
    std::vector<bool> alive_;
    std::map<std::string, std::vector<std::size_t>> by_recording_;
    std::map<long long, std::vector<std::size_t>> by_entry_;
    std::map<long long, std::set<std::string>> fresh_in_;
    std::set<long long> touched_;
    std::set<long long> split_;
    std::map<long long, long long> saved_ids_;
    long long next_new_ = -1;
    long long added_{};
};
}

std::string_view status_name(Status status) { return status_names[static_cast<std::size_t>(status)]; }

Status status_from_name(std::string_view name) {
    for (std::size_t index = 0; index < status_names.size(); ++index)
        if (status_names[index] == name) return static_cast<Status>(index);
    throw StoreCorrupt("Store has an entry with unknown status " + std::string(name));
}

bool status_is_final(Status status) {
    return status == Status::programme || status == Status::intro || status == Status::credits;
}

bool standard_length(double seconds) {
    return std::ranges::any_of(standard_lengths, [&](double length) { return std::abs(seconds - length) <= 1.0; });
}

bool audio_ad_length(double seconds) {
    return std::ranges::any_of(audio_lengths, [&](double length) { return std::abs(seconds - length) <= 1.0; });
}

Promotion promote(Status current, double length, std::span<const AiringFacts> airings, const PromotionRules& rules) {
    if (current == Status::programme) return {current, ""};
    std::set<std::string> series, broadcasts, recordings;
    // Distinct stations, as the network and channel pairs seen.
    std::set<std::pair<std::string, std::string>> places;
    for (const auto& airing : airings) {
        if (!airing.series_key.empty()) series.insert(airing.series_key);
        const auto& broadcast = airing.broadcast.empty() ? airing.series_key : airing.broadcast;
        if (!broadcast.empty()) broadcasts.insert(broadcast);
        places.emplace(airing.network, airing.channel);
        recordings.insert(airing.recording);
    }
    if (airings.size() < 2) {
        if (current == Status::intro || current == Status::credits)
            return {Status::programme, std::string(status_name(current)) + " left with one airing"};
        return {Status::candidate, "one airing"};
    }

    bool any_evidence = false, logo_free = true, logo_everywhere = true, mostly_logo = true, some_logo = false, seams_both = false;
    for (const auto& airing : airings) {
        if (!airing.has_evidence) continue;
        any_evidence = true;
        const bool seen = airing.logo_fraction >= logo_none_below;
        logo_free = logo_free && !seen;
        logo_everywhere = logo_everywhere && seen;
        mostly_logo = mostly_logo && airing.logo_fraction >= logo_in_programme;
        some_logo = some_logo || airing.logo_fraction >= logo_in_programme;
        seams_both = seams_both || (airing.seam_start && airing.seam_end);
    }
    // Evidence vetoes: a logo seen in any airing keeps the entry from being an ad
    // by recurrence alone.
    const bool no_logo_seen = logo_free;

    // Within one series, at positions that vary: not a theme, credits or recap.
    std::string varying_key;
    int varying_recordings = 0;
    for (const auto& key : series) {
        std::set<std::string> in_series, away;
        double first_start = 0, last_start = 0, first_end = 0, last_end = 0;
        bool any = false;
        int count = 0, starts = 0, ends = 0;
        for (const auto& airing : airings) {
            if (airing.series_key != key || !airing.has_position) continue;
            in_series.insert(airing.recording);
            if (airing.from_start >= recap_edge_seconds && airing.to_end >= recap_edge_seconds) away.insert(airing.recording);
            ++count;
            starts += airing.near_start;
            ends += airing.near_end;
            first_start = any ? std::min(first_start, airing.from_start) : airing.from_start;
            last_start = any ? std::max(last_start, airing.from_start) : airing.from_start;
            first_end = any ? std::min(first_end, airing.to_end) : airing.to_end;
            last_end = any ? std::max(last_end, airing.to_end) : airing.to_end;
            any = true;
        }
        // Opening credits over the first act, after cold opens of varying length,
        // vary too, but stay near the start.
        const bool one_end = 3 * starts > 2 * count || 3 * ends > 2 * count;
        const bool varies = std::min(last_start - first_start, last_end - first_end) > varying_position_seconds && !one_end;
        if (varies && static_cast<int>(away.size()) >= within_series_recordings && static_cast<int>(away.size()) > varying_recordings) {
            varying_key = key;
            varying_recordings = static_cast<int>(away.size());
        }
    }
    // Twice or more in one recording, minutes apart: a replay has its own
    // commentary, so audio that repeats within a broadcast is an ad or promo.
    std::map<std::string, std::vector<double>> starts_in;
    for (const auto& airing : airings)
        if (airing.has_position && airing.from_start >= recap_edge_seconds && airing.to_end >= recap_edge_seconds)
            starts_in[airing.recording].push_back(airing.from_start);
    std::string repeating;
    for (auto& [name, starts] : starts_in) {
        std::ranges::sort(starts);
        if (starts.size() >= 2 && starts.back() - starts.front() >= within_recording_apart) {
            repeating = name;
            break;
        }
    }
    const bool within_series = no_logo_seen && (!varying_key.empty() || !repeating.empty());
    const std::string within_reason =
        !varying_key.empty() ? "in " + std::to_string(varying_recordings) + " recordings of " + varying_key + " at varying positions"
                             : "twice in one recording, minutes apart";

    // Intro and credits come first, so a theme bounded by black frames is never
    // an ad. The series must hold at least half the entry's recordings: an ad
    // that happened to air early in two episodes of one show, and in a dozen
    // other shows, is not that show's theme.
    bool series_holds = false;
    for (const auto& key : series) {
        std::set<std::string> in_series;
        bool all_start = true, all_end = true;
        for (const auto& airing : airings) {
            if (airing.series_key != key) continue;
            in_series.insert(airing.recording);
            all_start = all_start && airing.near_start;
            all_end = all_end && airing.near_end;
        }
        if (in_series.size() < 2 || 2 * in_series.size() < recordings.size()) continue;
        series_holds = true;
        if (!(all_start || all_end)) continue;
        const Status kind = all_start ? Status::intro : Status::credits;
        Promotion promotion{kind, "near the " + std::string(all_start ? "start" : "end") + " of " + key + " in every airing"};
        promotion.ad_if_beside = no_logo_seen && audio_ad_length(length);
        return promotion;
    }
    // An ad that recurs within one series at varying positions: checked after
    // intro and credits, so a theme after a cold open of varying length stays one.
    if (within_series && audio_ad_length(length)) return {Status::ad, within_reason + ", no logo seen, an ad's length"};
    // An intro or credits entry that a later airing in its series breaks the rule
    // for is programme: it is a series' own audio, so it never becomes an ad.
    if ((current == Status::intro || current == Status::credits) && series_holds)
        return {Status::programme, std::string(status_name(current)) + " until an airing away from the start or end"};

    const auto spread = static_cast<int>(broadcasts.size());
    bool two_places = false;
    for (auto one = places.begin(); one != places.end() && !two_places; ++one)
        for (auto other = std::next(one); other != places.end() && !two_places; ++other) {
            AiringFacts first, second;
            std::tie(first.network, first.channel) = *one;
            std::tie(second.network, second.channel) = *other;
            two_places = different_places(first, second);
        }
    const bool across = spread >= 2 || two_places;
    const bool standard = standard_length(length);
    // Evidence confirms: Comskip saw no logo in an airing.
    if (any_evidence && across && logo_free && (standard || seams_both))
        return {Status::ad, standard ? "across shows, no logo, standard length" : "across shows, no logo, seams at both ends"};
    if (any_evidence && !across && logo_free && seams_both && standard)
        return {Status::ad, "within one show, no logo, seams at both ends, standard length"};
    if (any_evidence && across && mostly_logo) return {Status::in_programme_ad, "across shows, logo during most of every airing"};
    if (any_evidence && logo_everywhere && series.size() <= 1) return {Status::programme, "logo in every airing, one series"};
    const bool widely = spread >= rules.audio_series || (spread >= 2 && two_places);
    if (no_logo_seen && widely && audio_ad_length(length))
        return {Status::ad, "in " + std::to_string(spread) + " series, no logo seen, an ad's length"};
    const bool may_join = no_logo_seen && spread >= 2 && length <= adjacent_max_length;
    // Shown, never skipped: recurring segments under a logo, or recurring widely
    // at other lengths (stings, billboards, promos).
    if (any_evidence && across && some_logo) return {Status::in_programme_ad, "across shows, logo during most of an airing", may_join};
    if (no_logo_seen && widely)
        return {Status::in_programme_ad, "in " + std::to_string(spread) + " series, not an ad's length", may_join, true};
    // Pieces of a split ad within one series: judged with the airings they abut.
    if (within_series) return {Status::candidate, within_reason + ", not an ad's length", may_join, true};
    return {Status::candidate, "no rule holds", may_join};
}

void Library::add_stream(const std::string& recording, const Context& context, std::span<const CodeRun> runs) {
    in_transaction(store_, [&] {
        store_.prepare(
                  "INSERT INTO recording(id, source, channel, air_start, context) VALUES(?, ?, ?, ?, ?) "
                  "ON CONFLICT(id) DO UPDATE SET source = excluded.source, channel = excluded.channel, "
                  "air_start = excluded.air_start, context = excluded.context")
            .bind(1, recording)
            .bind(2, context.source)
            .bind(3, context.channel)
            .bind(4, context.air_start)
            .bind(5, context_to_json(context))
            .run();
        auto existing = store_.prepare("SELECT length(codes) FROM run WHERE recording = ? AND start = ?");
        auto insert = store_.prepare(
            "INSERT INTO run(recording, start, codes) VALUES(?, ?, ?) "
            "ON CONFLICT(recording, start) DO UPDATE SET codes = excluded.codes");
        for (const auto& run : runs) {
            existing.bind(1, recording).bind(2, run.start);
            const bool stored = existing.step();
            const auto stored_codes = stored ? existing.integer(0) / 4 : 0;
            existing.reset();
            if (stored && stored_codes >= static_cast<long long>(run.codes.size())) continue;
            insert.bind(1, recording).bind(2, run.start).bind_codes(3, run.codes).run();
        }
    });
}

bool Library::replace_context(const std::string& recording, const Context& context) {
    bool stored = false;
    in_transaction(store_, [&] {
        store_.prepare("UPDATE recording SET source = ?, channel = ?, air_start = ?, context = ? WHERE id = ?")
            .bind(1, context.source)
            .bind(2, context.channel)
            .bind(3, context.air_start)
            .bind(4, context_to_json(context))
            .bind(5, recording)
            .run();
        stored = has_stream(recording);
    });
    return stored;
}

bool Library::has_stream(const std::string& recording) {
    auto query = store_.prepare("SELECT 1 FROM recording WHERE id = ?");
    query.bind(1, recording);
    return query.step();
}

RepeatsResult Library::promote_all() {
    Search search(store_, options_, "");
    return search.promote_all();
}

RepeatsResult Library::find_repeats(const std::string& recording, const Evidence* evidence) {
    Search search(store_, options_, recording);
    auto result = search.run(evidence);
    result.airings = matches(recording);
    return result;
}

std::vector<LibraryMatch> Library::matches(const std::string& recording) {
    std::vector<LibraryMatch> found;
    auto query = store_.prepare(
        "SELECT airing.start, airing.\"end\", entry.status, entry.id FROM airing JOIN entry ON entry.id = airing.entry "
        "WHERE airing.recording = ? ORDER BY airing.start, entry.id");
    query.bind(1, recording);
    while (query.step()) {
        const auto status = status_from_name(query.text(2));
        const bool marked = status == Status::ad || status == Status::in_programme_ad ||
                            (options_.intro_credits && (status == Status::intro || status == Status::credits));
        if (marked) found.push_back({query.real(0), query.real(1), status, query.integer(3)});
    }
    return found;
}

std::vector<Airing> Library::airings(const std::string& recording) {
    std::vector<Airing> found;
    auto query = store_.prepare(
        "SELECT entry, run_start, code_begin, code_end, part_begin, part_end, logo, logo_fraction, seam_start, seam_end "
        "FROM airing WHERE recording = ? ORDER BY start");
    query.bind(1, recording);
    while (query.step()) {
        Airing airing;
        airing.entry = query.integer(0);
        airing.recording = recording;
        airing.run_start = query.real(1);
        airing.code_begin = static_cast<long>(query.integer(2));
        airing.code_end = static_cast<long>(query.integer(3));
        airing.part_begin = static_cast<long>(query.integer(4));
        airing.part_end = static_cast<long>(query.integer(5));
        airing.logo = query.text(6);
        if (!query.is_null(7)) airing.logo_fraction = query.real(7);
        if (!query.is_null(8)) airing.seam_start = query.integer(8) != 0;
        if (!query.is_null(9)) airing.seam_end = query.integer(9) != 0;
        found.push_back(std::move(airing));
    }
    return found;
}

std::vector<EntryInfo> Library::entries() {
    std::vector<EntryInfo> found;
    auto query = store_.prepare("SELECT id, status, length, reason, airings, codes, coalesce(last_airing, 0) FROM entry ORDER BY id");
    while (query.step())
        found.push_back({query.integer(0), status_from_name(query.text(1)), query.real(2), query.text(3), query.integer(4),
                         query.codes(5), query.real(6)});
    return found;
}

std::vector<EntryInfo> Library::live_index() {
    std::vector<EntryInfo> found;
    live_index(live_index_bytes, [&](EntryInfo&& entry) { found.push_back(std::move(entry)); });
    return found;
}

void Library::live_index(std::size_t max_bytes, const std::function<void(EntryInfo&&)>& visit) {
    auto query = store_.prepare(
        "SELECT id, status, length, reason, airings, codes, last_airing FROM entry "
        "WHERE status IN ('ad', 'in-programme ad', 'intro', 'credits') ORDER BY last_airing DESC, id");
    std::size_t bytes = 0;
    while (query.step()) {
        EntryInfo entry{query.integer(0), status_from_name(query.text(1)), query.real(2), query.text(3), query.integer(4),
                        query.codes(5), query.real(6)};
        bytes += live_index_entry_bytes(entry.codes.size());
        if (bytes > max_bytes) break;
        visit(std::move(entry));
    }
}

std::vector<std::tuple<long long, Status, long long>> Library::live_index_key(std::size_t max_bytes) {
    std::vector<std::tuple<long long, Status, long long>> key;
    auto query = store_.prepare(
        "SELECT id, status, length(codes) FROM entry "
        "WHERE status IN ('ad', 'in-programme ad', 'intro', 'credits') ORDER BY last_airing DESC, id");
    std::size_t bytes = 0;
    while (query.step()) {
        bytes += live_index_entry_bytes(static_cast<std::size_t>(query.integer(2) / 4));
        if (bytes > max_bytes) break;
        key.emplace_back(query.integer(0), status_from_name(query.text(1)), query.integer(2) / 4);
    }
    return key;
}
}
