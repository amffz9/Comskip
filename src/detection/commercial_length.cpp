#include "commercial_length.h"
#include <cmath>
#include <algorithm>
#include <span>

bool commercial_length_within_tolerance(double length, double expected,
                                       double tolerance, double fps)
{
    if (!std::isfinite(length * fps) || !std::isfinite(expected * fps) ||
        !std::isfinite(tolerance * fps) || fps <= 0 || tolerance < 0)
        return false;
    // Preserve whole-frame truncation without narrowing to int or overflowing
    // integer subtraction on unusually long inputs.
    return std::abs(std::trunc(length * fps) - std::trunc(expected * fps))
        <= std::trunc(tolerance * fps);
}

bool commercial_length_match(double length, double tolerance, bool strict,
                             const comskip::config::CommercialProfile& profile,
                             const CommercialLengthPolicy& policy,
                             CommercialLengthMatch& match)
{
    double local_tolerance = policy.tolerance_override >= 0
        ? policy.tolerance_override : tolerance;

    local_tolerance = std::clamp(local_tolerance, profile.minimum_tolerance, profile.maximum_tolerance);
    length += profile.correction;
    const auto matches = [&](std::span<const int> lengths) {
        for (int expected : lengths) {
            if (expected < policy.min_show_segment_length - profile.show_margin &&
                commercial_length_within_tolerance(length, expected, local_tolerance, policy.fps)) {
                match = CommercialLengthMatch{length, length - expected, local_tolerance};
                return true;
            }
        }
        return false;
    };
    return matches(profile.strict_lengths) || (!strict && matches(profile.optional_lengths));
}
