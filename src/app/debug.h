#pragma once

#include <string_view>

struct RecordingContext;

// Application diagnostic sink. Callers own translated message selection.
void Debug(RecordingContext& context, int level, std::string_view message);
void Debug(RecordingContext& context, int level, const char* format, ...);
