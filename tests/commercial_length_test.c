#include "commercial_length.h"
#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

int main(void)
{
    CommercialLengthPolicy policy = { 25.0, -1.0, 120.0 };
    CommercialLengthMatch match;
    const double rates[] = { 23.976, 25.0, 29.97, 50.0, 59.94 };
    unsigned int i;

    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        policy.fps = rates[i];
        CHECK(commercial_length_match(30.0, 0.5, 1, &policy, &match));
        CHECK(!commercial_length_match(32.0, 0.5, 1, &policy, &match));
    }
    policy.fps = 25.0;
    /* Inclusive tolerance, with the existing whole-frame truncation. */
    CHECK(commercial_length_within_tolerance(30.48, 30.0, 0.5, 25.0));
    CHECK(!commercial_length_within_tolerance(30.52, 30.0, 0.5, 25.0));
    CHECK(commercial_length_within_tolerance(29.52, 30.0, 0.5, 25.0));
    CHECK(!commercial_length_within_tolerance(29.48, 30.0, 0.5, 25.0));

    CHECK(!commercial_length_match(5.0, 0.5, 1, &policy, &match));
    CHECK(commercial_length_match(5.0, 0.5, 0, &policy, &match));
    CHECK(!commercial_length_match(35.0, 0.5, 1, &policy, &match));
    CHECK(commercial_length_match(35.0, 0.5, 0, &policy, &match));
#ifdef CHINESE_SIZE_TABLE
    CHECK(commercial_length_match(18.0, 0.5, 1, &policy, &match));
    CHECK(commercial_length_match(72.0, 0.5, 1, &policy, &match));
#else
    CHECK(!commercial_length_match(18.0, 0.5, 1, &policy, &match));
    CHECK(!commercial_length_match(72.0, 0.5, 1, &policy, &match));
#endif

    CHECK(commercial_length_match(29.89, 0.0, 1, &policy, &match));
    CHECK(fabs(match.adjusted_length - 30.0) < 0.000001);
    CHECK(fabs(match.delta) < 0.000001);
    CHECK(match.tolerance == 0.5);
    CHECK(commercial_length_match(30.7, 9.0, 1, &policy, &match));
    CHECK(match.tolerance == 1.0);
    CHECK(!commercial_length_match(31.2, 9.0, 1, &policy, &match));

    policy.tolerance_override = 0.5;
    CHECK(!commercial_length_match(30.7, 1.0, 1, &policy, &match));
    policy.tolerance_override = 1.0;
    CHECK(commercial_length_match(30.7, 0.5, 1, &policy, &match));

    policy.min_show_segment_length = 33.0;
    CHECK(!commercial_length_match(30.0, 0.5, 1, &policy, &match));
    policy.min_show_segment_length = 33.01;
    CHECK(commercial_length_match(30.0, 0.5, 1, &policy, &match));
    match.adjusted_length = -123.0;
    CHECK(!commercial_length_match(200.0, 0.5, 1, &policy, &match));
    CHECK(match.adjusted_length == -123.0);

    if (failures)
        return 1;
    puts("Commercial length tests passed");
    return 0;
}
