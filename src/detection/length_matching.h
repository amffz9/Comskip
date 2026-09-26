#pragma once

struct RecordingContext;

bool IsStandardCommercialLength(RecordingContext& context, double length,
                                double tolerance, bool strict);
