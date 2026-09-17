#pragma once

#include <cstdio>

struct RecordingContext;
namespace comskip::localization { class Translator; }

// Legacy configuration reload entry point used by the review workflow.
void LoadIniFile(RecordingContext& context);
void LoadIniFile(RecordingContext& context, const comskip::localization::Translator& translator);
void LoadSettings(RecordingContext& context, int argc, char** argv,
    const comskip::localization::Translator& translator);

// Formatting remains tied to the per-recording scratch buffer for compatibility.
char* intSecondsToStrMinutes(RecordingContext& context, int seconds);
char* dblSecondsToStrMinutes(RecordingContext& context, double seconds);
char* dblSecondsToStrMinutesFrames(RecordingContext& context, double seconds);
