#include "detection/reference_comparison.h"
#include <optional>
#include <stdexcept>

namespace comskip::detection {
namespace {
using comskip::output::CommercialInterval;
using comskip::output::FrameIndex;
void validate(std::span<const CommercialInterval> intervals) {
    FrameIndex previous_end = 0;
    for (const auto& interval : intervals) {
        if (interval.start_frame < previous_end || interval.end_frame < interval.start_frame)
            throw std::invalid_argument("Reference comparison requires ordered disjoint intervals");
        previous_end = interval.end_frame;
    }
}
bool nearby(FrameIndex first, FrameIndex second, FrameIndex tolerance) {
    return (first >= second ? first - second : second - first) < tolerance;
}
}
std::vector<ReferenceComparisonEvent> compare_reference_intervals(
    std::span<const CommercialInterval> reference,
    std::span<const CommercialInterval> detected, FrameIndex tolerance) {
    if (tolerance < 0) throw std::invalid_argument("Negative reference comparison tolerance");
    validate(reference); validate(detected);
    std::vector<ReferenceComparisonEvent> events;
    if (reference.empty() && detected.empty()) return events;
    const auto horizon = detected.empty() ? reference.back().end_frame : detected.back().end_frame;
    std::size_t reference_index = 0, detected_index = 0;
    FrameIndex position = 0;
    enum class State { both_show, both_commercial, only_reference, only_detected };
    auto state = State::both_show;
    const auto entered_reference = [&] {
        if (reference_index < reference.size()) {
            const auto& interval = reference[reference_index];
            if (interval.end_frame - interval.start_frame > 2)
                events.push_back({ReferenceEventKind::reference_duration, interval});
        }
    };
    entered_reference();
    while (position < horizon && (reference_index < reference.size() || detected_index < detected.size())) {
        const auto* expected = reference_index < reference.size() ? &reference[reference_index] : nullptr;
        const auto* found = detected_index < detected.size() ? &detected[detected_index] : nullptr;
        const auto previous_position = position;
        switch (state) {
        case State::both_show:
            if (expected && found && nearby(expected->start_frame, found->start_frame, tolerance)) {
                state = State::both_commercial; position = found->start_frame;
            } else if (found && (!expected || found->start_frame < expected->start_frame)) {
                state = State::only_detected; position = found->start_frame;
            } else if (expected) {
                state = State::only_reference; position = expected->start_frame;
            }
            break;
        case State::both_commercial:
            if (expected && found && nearby(expected->end_frame, found->end_frame, tolerance)) {
                state = State::both_show; position = found->end_frame;
                ++reference_index; entered_reference(); ++detected_index;
            } else if (found && (!expected || found->end_frame < expected->end_frame)) {
                state = State::only_reference; position = found->end_frame; ++detected_index;
            } else if (expected) {
                state = State::only_detected; position = expected->end_frame;
                ++reference_index; entered_reference();
            }
            break;
        case State::only_reference:
            if (expected && (!found || expected->end_frame < found->start_frame)) {
                state = State::both_show; position = expected->end_frame;
                ++reference_index; entered_reference();
            } else if (found) {
                state = State::both_commercial; position = found->start_frame;
            } else {
                throw std::logic_error("Reference comparison lost its active interval");
            }
            events.push_back({ReferenceEventKind::false_negative, {previous_position, position}});
            break;
        case State::only_detected:
            if (found && (!expected || found->end_frame < expected->start_frame)) {
                state = State::both_show; position = found->end_frame; ++detected_index;
            } else if (expected) {
                state = State::both_commercial; position = expected->start_frame;
            } else {
                throw std::logic_error("Reference comparison lost its active interval");
            }
            events.push_back({ReferenceEventKind::false_positive, {previous_position, position}});
            break;
        }
    }
    return events;
}
}
