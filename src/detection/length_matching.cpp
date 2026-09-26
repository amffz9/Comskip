#include "app/recording_context.h"
#include "detection/commercial_length.h"
#include "output/cutlist_exports.h"

bool IsStandardCommercialLength(RecordingContext& context, double length, double tolerance, bool strict)
{
    CommercialLengthPolicy policy = { context.settings.fps, context.settings.div5_tolerance, context.settings.min_show_segment_length };
    CommercialLengthMatch match;
    if (!commercial_length_match(length, tolerance, strict, context.settings.commercial_profile, policy, match))
        return false;
    OutputStrict(context, match.adjusted_length, match.delta, match.tolerance);
    return true;
}

