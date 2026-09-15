#include "legacy_detection.h"

bool LengthWithinTolerance(double test_length, double expected_length, double tolerance)
{
    return commercial_length_within_tolerance(test_length, expected_length, tolerance, fps);
}

bool IsStandardCommercialLength(double length, double tolerance, bool strict)
{
    CommercialLengthPolicy policy = { fps, div5_tolerance, min_show_segment_length };
    CommercialLengthMatch match;
    if (!commercial_length_match(length, tolerance, strict, &policy, &match))
        return false;
    OutputStrict(match.adjusted_length, match.delta, match.tolerance);
    return true;
}

