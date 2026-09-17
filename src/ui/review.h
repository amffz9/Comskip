#pragma once

struct RecordingContext;

// Runs the optional interactive review session and returns whether to reload.
bool ReviewResult(RecordingContext& context);