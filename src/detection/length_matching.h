#pragma once

struct RecordingContext;

bool LengthWithinTolerance(RecordingContext& context, double test_length,
                           double expected_length, double tolerance);
bool IsStandardCommercialLength(RecordingContext& context, double length,
                                double tolerance, bool strict);
