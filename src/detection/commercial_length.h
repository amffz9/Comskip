#pragma once
#include "profile.h"

/* Durations are in seconds. Keep timing policy independent of decoder/UI state. */
struct CommercialLengthPolicy {
    double fps;
    double tolerance_override; /* Negative means use the caller's tolerance. */
    double min_show_segment_length;
};

struct CommercialLengthMatch {
    double adjusted_length;
    double delta;
    double tolerance;
};

bool commercial_length_within_tolerance(double length, double expected,
                                       double tolerance, double fps);
/* match is written only on success; strict excludes the optional lengths. */
bool commercial_length_match(double length, double tolerance, bool strict,
                             const comskip::config::CommercialProfile& profile,
                             const CommercialLengthPolicy& policy,
                             CommercialLengthMatch& match);
