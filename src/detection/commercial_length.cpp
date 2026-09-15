#include "commercial_length.h"
#include <cstdlib>
#include <algorithm>
#include "profile.h"

int commercial_length_within_tolerance(double length, double expected,
                                      double tolerance, double fps)
{
    /* Preserve the detector's truncation to whole frames. */
    return abs((int)(length * fps) - (int)(expected * fps))
        <= (int)(tolerance * fps);
}

int commercial_length_match(double length, double tolerance, int strict,
                            const CommercialLengthPolicy *policy,
                            CommercialLengthMatch *match)
{
    const auto& profile = comskip::config::commercial_profile();
    auto lengths = profile.strict_lengths;
    if (!strict) lengths.insert(lengths.end(), profile.optional_lengths.begin(), profile.optional_lengths.end());
    double local_tolerance = policy->tolerance_override >= 0
        ? policy->tolerance_override : tolerance;

    local_tolerance = std::clamp(local_tolerance, profile.minimum_tolerance, profile.maximum_tolerance);
    length += profile.correction;
    for (int expected : lengths) {
        if (expected < policy->min_show_segment_length - profile.show_margin &&
            commercial_length_within_tolerance(length, expected,
                                               local_tolerance, policy->fps)) {
            match->adjusted_length = length;
            match->delta = length - expected;
            match->tolerance = local_tolerance;
            return 1;
        }
    }
    return 0;
}
