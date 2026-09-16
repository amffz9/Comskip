#include "../localization/diagnostic.h"
#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>

namespace comskip::ui {
enum class IntervalDirection { next, previous };

// A missing boundary leaves the review cursor at its current position.
// The five-frame margin preserves the review keys' existing skip behavior.
template<class Interval>
std::optional<long> review_interval_boundary(std::span<const Interval> storage,
                                            int last, long current,
                                            IntervalDirection direction) {
    if (last < -1 || (last >= 0 && static_cast<std::size_t>(last) >= storage.size()))
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::review_interval_count_exceeds_stored_intervals);
    const auto intervals = storage.first(static_cast<std::size_t>(last + 1));
    const auto position = static_cast<std::int64_t>(current);
    if (direction == IntervalDirection::next) {
        if (position > std::numeric_limits<std::int64_t>::max() - 5) return std::nullopt;
        const auto found = std::ranges::find_if(intervals, [&](const auto& value) {
            return static_cast<std::int64_t>(value.end_frame) >= position + 5;
        });
        if (found != intervals.end()) return found->end_frame;
    } else {
        if (position < std::numeric_limits<std::int64_t>::min() + 5) return std::nullopt;
        for (auto it = intervals.rbegin(); it != intervals.rend(); ++it)
            if (static_cast<std::int64_t>(it->start_frame) <= position - 5)
                return it->start_frame;
    }
    return std::nullopt;
}
}
