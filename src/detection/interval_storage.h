#include "../localization/diagnostic.h"
#pragma once
#include "detector_records.h"
#include <algorithm>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace comskip::detection {
template<class Entry>
void validate_intervals(const std::vector<Entry>& intervals, int last) {
    if (last < -1 || intervals.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        static_cast<std::size_t>(static_cast<long long>(last) + 1) != intervals.size())
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::interval_count_does_not_match_owned_storage);
}
template<class Entry>
void reset_intervals(std::vector<Entry>& intervals, int& last) noexcept {
    intervals.clear();
    last = -1;
}
template<class Entry>
void append_interval(std::vector<Entry>& intervals, int& last, Entry entry) {
    validate_intervals(intervals, last);
    if (intervals.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::interval_count_exceeds_the_supported_index_type);
    intervals.push_back(entry);
    last = static_cast<int>(intervals.size()) - 1;
}
template<class Entry>
void erase_interval(std::vector<Entry>& intervals, int& last, int index) {
    validate_intervals(intervals, last);
    if (index < 0 || index > last) throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_interval_removal);
    intervals.erase(intervals.begin() + index);
    last = static_cast<int>(intervals.size()) - 1;
}

// Insert within the surrounding uncovered gap and the recording's frame bounds.
inline bool insert_reference(std::vector<Legacy_reffer_entry>& intervals, int& last,
                             long frame, long frames) {
    validate_intervals(intervals, last);
    if (frame < 1 || frame > frames) return false;
    auto next = std::upper_bound(intervals.begin(), intervals.end(), frame,
        [](long position, const auto& entry) { return position < entry.start_frame; });
    if (next != intervals.begin() && std::prev(next)->end_frame >= frame) return false;
    const long lower = next == intervals.begin() ? 1 : std::prev(next)->end_frame + 1;
    const long upper = next == intervals.end() ? frames : next->start_frame - 1;
    const long start = frame - std::min(frame - lower, 1000L);
    const long end = frame + std::min(upper - frame, 1000L);
    if (intervals.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::reference_count_exceeds_the_supported_index_type);
    intervals.insert(next, {start, end});
    last = static_cast<int>(intervals.size()) - 1;
    return true;
}
struct LiveCandidate {
    long start;
    long end;
    int start_index;
    int end_index;
};
}
