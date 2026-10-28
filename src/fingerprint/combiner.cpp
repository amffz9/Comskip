#include "fingerprint/combiner.h"
#include "fingerprint/json.h"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace comskip::fingerprint {
namespace {
struct Stretch {
    double from{}, to{};
    bool fingerprint{};
    bool comskip{};
    bool unconfirmed{};  // projected, joined or extended: provisional while live
};

// The nearest seam within reach of a time, or the time itself.
double snap(const Evidence& evidence, double time) {
    double best = time;
    double distance = snap_reach;
    const auto first = std::ranges::lower_bound(evidence.seams, time - snap_reach, {}, &Seam::at);
    for (auto seam = first; seam != evidence.seams.end() && seam->at <= time + snap_reach; ++seam) {
        if (std::abs(seam->at - time) <= distance) {
            distance = std::abs(seam->at - time);
            best = seam->at;
        }
    }
    return best;
}

// Rule 4: programme evidence between two stretches closes the first: a logo
// interval of at least 5 s that reaches at least 1 s into the gap, or a Comskip
// programme block that begins inside it.
bool programme_between(const Evidence& evidence, double from, double to) {
    for (const auto& logo : evidence.logo) {
        if (logo.to - logo.from < logo_closes) continue;
        if (std::min(to, logo.to) - std::max(from, logo.from) >= touch_reach) return true;
    }
    for (const auto& block : evidence.blocks)
        if (!block.commercial && block.from > from - touch_reach && block.from < to) return true;
    return false;
}

// Comskip's adjacent blocks are a frame apart; its cut list makes them one cut.
constexpr double adjacent = 0.1;

// An ad's audio over programme, such as g4's Google split-screen, whose audio
// aired as an ordinary ad in g5: the logo stops at most 8 s before the match and
// returns at most 8 s after it, and seams don't bound it at both ends as they
// bound a break of one spot.
bool over_programme(const Evidence& evidence, double from, double to) {
    if (!evidence.logo_known) return false;
    bool before = false, after = false;
    for (const auto& logo : evidence.logo) {
        if (logo.to - logo.from < logo_closes) continue;
        before = before || (logo.to <= from + touch_reach && logo.to >= from - bracket_reach);
        after = after || (logo.from >= to - touch_reach && logo.from <= to + bracket_reach);
    }
    const auto seam = [&](double time) {
        return std::ranges::any_of(evidence.seams, [&](const Seam& s) { return std::abs(s.at - time) <= snap_reach; });
    };
    return before && after && !(seam(from) && seam(to));
}

void merge_overlapping(std::vector<Stretch>& stretches) {
    std::ranges::sort(stretches, {}, &Stretch::from);
    std::vector<Stretch> merged;
    for (const auto& stretch : stretches) {
        if (!merged.empty() && stretch.from <= merged.back().to + adjacent) {
            auto& last = merged.back();
            last.to = std::max(last.to, stretch.to);
            last.fingerprint = last.fingerprint || stretch.fingerprint;
            last.comskip = last.comskip || stretch.comskip;
            last.unconfirmed = last.unconfirmed || stretch.unconfirmed;
        } else {
            merged.push_back(stretch);
        }
    }
    stretches = std::move(merged);
}

// Pieces of from..to outside every ad mark.
std::vector<std::pair<double, double>> outside(double from, double to, const std::vector<Mark>& ads) {
    std::vector<std::pair<double, double>> pieces{{from, to}};
    for (const auto& ad : ads) {
        std::vector<std::pair<double, double>> next;
        for (const auto& [a, b] : pieces) {
            if (ad.end <= a || ad.start >= b) {
                next.emplace_back(a, b);
                continue;
            }
            if (ad.start > a) next.emplace_back(a, ad.start);
            if (ad.end < b) next.emplace_back(ad.end, b);
        }
        pieces = std::move(next);
    }
    return pieces;
}

MarkState state_of(const CombineMode& mode, bool unconfirmed, double end) {
    if (mode.final) return MarkState::settled;
    if (!unconfirmed && end <= mode.now) return MarkState::settled;
    return end + settle_after <= mode.now ? MarkState::settled : MarkState::provisional;
}
}

std::string_view kind_name(MarkKind kind) {
    switch (kind) {
    case MarkKind::ad: return "ad";
    case MarkKind::in_programme_ad: return "in-programme ad";
    case MarkKind::intro: return "intro";
    case MarkKind::credits: return "credits";
    }
    return "ad";
}
std::string_view state_name(MarkState state) { return state == MarkState::settled ? "settled" : "provisional"; }
std::string_view basis_name(Basis basis) {
    switch (basis) {
    case Basis::fingerprint: return "fingerprint";
    case Basis::comskip: return "comskip";
    case Basis::both: return "both";
    }
    return "both";
}

CombinerMatch agreed_match(double from, double to, Status status) { return {from, to, from, to, status}; }

std::vector<Mark> combine(std::span<const CombinerMatch> matches, const Evidence* evidence_pointer, CombineMode mode) {
    if (!evidence_pointer) throw EvidenceMissing("No evidence for the capture");
    const auto& evidence = *evidence_pointer;

    // Rule 1: ad matches become stretches over agreed plus projected parts, their
    // edges moved to a seam within 1 s.
    std::vector<Stretch> breaks;
    std::vector<Mark> others;
    for (const auto& match : matches) {
        if (match.status == Status::ad && over_programme(evidence, match.agreed_from, match.agreed_to)) {
            const double from = snap(evidence, match.from);
            const double to = snap(evidence, match.to);
            others.push_back({from, to, MarkKind::in_programme_ad, state_of(mode, true, to), Basis::fingerprint});
        } else if (match.status == Status::ad) {
            const bool projected = match.from < match.agreed_from || match.to > match.agreed_to;
            breaks.push_back({snap(evidence, match.from), snap(evidence, match.to), true, false, projected});
        } else if (match.status == Status::in_programme_ad) {
            const double from = snap(evidence, match.from);
            const double to = snap(evidence, match.to);
            const bool projected = match.from < match.agreed_from || match.to > match.agreed_to;
            others.push_back({from, to, MarkKind::in_programme_ad, state_of(mode, projected, to), Basis::fingerprint});
        } else if (match.status == Status::intro || match.status == Status::credits) {
            // Rule 7: the agreed part only, never joined to or closing a break.
            const auto kind = match.status == Status::intro ? MarkKind::intro : MarkKind::credits;
            others.push_back({match.agreed_from, match.agreed_to, kind, state_of(mode, false, match.agreed_to), Basis::fingerprint});
        }
    }
    std::erase_if(breaks, [](const Stretch& stretch) { return stretch.to <= stretch.from; });
    merge_overlapping(breaks);

    // Rule 2 (closed by rule 4): stretches less than 90 s apart, or 30 s when no
    // logo was learned, join when no programme evidence lies between.
    const double gap_limit = evidence.logo_known ? join_gap : join_gap_without_logo;
    std::vector<Stretch> joined;
    for (const auto& stretch : breaks) {
        if (!joined.empty()) {
            auto& last = joined.back();
            const double gap = stretch.from - last.to;
            if (gap < gap_limit && !programme_between(evidence, last.to, stretch.from)) {
                last.to = stretch.to;
                last.unconfirmed = true;
                continue;
            }
        }
        joined.push_back(stretch);
    }

    // Rule 3: a break extends back and forward over Comskip commercial blocks that
    // touch it, to those blocks' edges.
    std::vector<Block> commercial;
    for (const auto& block : evidence.blocks)
        if (block.commercial && block.to > block.from) commercial.push_back(block);
    for (auto& stretch : joined) {
        for (bool grew = true; grew;) {
            grew = false;
            for (const auto& block : commercial) {
                if (block.to < stretch.from - touch_reach || block.from > stretch.to + touch_reach) continue;
                stretch.comskip = true;
                if (block.from < stretch.from || block.to > stretch.to) {
                    stretch.from = std::min(stretch.from, block.from);
                    stretch.to = std::max(stretch.to, block.to);
                    stretch.unconfirmed = true;
                    grew = true;
                }
            }
        }
    }

    // Rule 5: every Comskip commercial block becomes an ad mark, as today.
    for (const auto& block : commercial) joined.push_back({block.from, block.to, false, true, true});
    // Rule 6: ad marks never overlap (adjacent ones are one mark, as in Comskip's
    // cut list).
    merge_overlapping(joined);

    std::vector<Mark> marks;
    for (const auto& stretch : joined) {
        const auto basis = stretch.fingerprint ? (stretch.comskip ? Basis::both : Basis::fingerprint) : Basis::comskip;
        marks.push_back({stretch.from, stretch.to, MarkKind::ad, state_of(mode, stretch.unconfirmed, stretch.to), basis});
    }
    const auto ads = marks;
    for (const auto& other : others) {
        double inside = 0;
        for (const auto& ad : ads) inside += std::max(0.0, std::min(other.end, ad.end) - std::max(other.start, ad.start));
        // Rule 6: an in-programme ad inside an ad mark is dropped. Rule 7: an ad
        // overlapping intro or credits wins; they are cut around it.
        if (other.kind == MarkKind::in_programme_ad && inside >= 0.5 * (other.end - other.start)) continue;
        for (const auto& [from, to] : outside(other.start, other.end, ads)) {
            if (to - from < 1.0) continue;
            auto piece = other;
            piece.start = from;
            piece.end = to;
            marks.push_back(piece);
        }
    }
    std::ranges::sort(marks, [](const Mark& x, const Mark& y) { return std::tie(x.start, x.end) < std::tie(y.start, y.end); });
    return marks;
}

std::string marks_file_json(std::string_view capture_id, long long evidence_revision, bool final, std::span<const Mark> marks) {
    std::string text = "{ \"captureId\": ";
    json::append_string(text, capture_id);
    text += ", \"evidenceRevision\": ";
    json::append_integer(text, evidence_revision);
    text += final ? ", \"final\": true,\n  \"marks\": [" : ", \"final\": false,\n  \"marks\": [";
    for (std::size_t index = 0; index < marks.size(); ++index) {
        const auto& mark = marks[index];
        text += index ? ",\n    { \"start\": " : "\n    { \"start\": ";
        json::append_number(text, mark.start, 3);
        text += ", \"end\": ";
        json::append_number(text, mark.end, 3);
        text += ", \"kind\": ";
        json::append_string(text, kind_name(mark.kind));
        text += ", \"state\": ";
        json::append_string(text, state_name(mark.state));
        text += ", \"basis\": ";
        json::append_string(text, basis_name(mark.basis));
        text += " }";
    }
    text += marks.empty() ? "] }\n" : " ] }\n";
    return text;
}
}
