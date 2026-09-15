#include "commercial_length.h"
#include <stdlib.h>

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
#ifdef CHINESE_SIZE_TABLE
    static const int lengths[] = {
        10, 15, 18, 20, 25, 30, 36, 45, 60, 72, 90, 108, 120, 126,
        150, 180, 5, 35, 40, 50, 70, 75
    };
    const int strict_count = 16;
#else
    static const int lengths[] = {
        10, 15, 20, 25, 30, 45, 60, 90, 120, 150, 180,
        5, 35, 40, 50, 70, 75
    };
    const int strict_count = 11;
#endif
    int count = strict ? strict_count : (int)(sizeof(lengths) / sizeof(lengths[0]));
    int i;
    double local_tolerance = policy->tolerance_override >= 0
        ? policy->tolerance_override : tolerance;

    if (local_tolerance < 0.5)
        local_tolerance = 0.5;
    if (local_tolerance > 1.0)
        local_tolerance = 1.0;

    /* Historical correction used by the detection heuristics. */
    length += 0.11;
    for (i = 0; i < count; ++i) {
        if (lengths[i] < policy->min_show_segment_length - 3 &&
            commercial_length_within_tolerance(length, lengths[i],
                                               local_tolerance, policy->fps)) {
            match->adjusted_length = length;
            match->delta = length - lengths[i];
            match->tolerance = local_tolerance;
            return 1;
        }
    }
    return 0;
}
