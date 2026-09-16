#pragma once

struct RecordingContext;

// Application diagnostic sink. Callers own translated message selection.
void Debug(RecordingContext& context, int level, const char* format, ...);
