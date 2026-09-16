#pragma once
#include "output/edl.h"
#include <span>
#include <vector>

namespace comskip::detection {
enum class ReferenceEventKind { reference_duration, false_positive, false_negative };
struct ReferenceComparisonEvent {
    ReferenceEventKind kind{};
    comskip::output::CommercialInterval interval;
};
// Inputs are ordered, disjoint, nonnegative frame intervals. The historical
// boundary tolerance and detected-list comparison horizon are retained. When
// detection is empty, the reference horizon supplies meaningful missed scores.
// Returns events in the legacy training-report order without changing inputs.
std::vector<ReferenceComparisonEvent> compare_reference_intervals(
    std::span<const comskip::output::CommercialInterval> reference,
    std::span<const comskip::output::CommercialInterval> detected,
    comskip::output::FrameIndex tolerance = 40);
}
