#ifndef COMSKIP_COMMERCIAL_LENGTH_H
#define COMSKIP_COMMERCIAL_LENGTH_H

/* Durations are in seconds. Keep timing policy independent of decoder/UI state. */
typedef struct {
    double fps;
    double tolerance_override; /* Negative means use the caller's tolerance. */
    double min_show_segment_length;
} CommercialLengthPolicy;

typedef struct {
    double adjusted_length;
    double delta;
    double tolerance;
} CommercialLengthMatch;

int commercial_length_within_tolerance(double length, double expected,
                                      double tolerance, double fps);
/* match is written only on success; strict excludes the optional lengths. */
int commercial_length_match(double length, double tolerance, int strict,
                            const CommercialLengthPolicy *policy,
                            CommercialLengthMatch *match);

#endif
