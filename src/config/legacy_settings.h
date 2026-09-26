#pragma once

#include <string>

struct RecordingContext;
namespace comskip::localization { class Translator; }

// Legacy configuration reload entry point used by the review workflow.
void LoadIniFile(RecordingContext& context);
void LoadIniFile(RecordingContext& context, const comskip::localization::Translator& translator);
void LoadSettings(RecordingContext& context, int argc, char** argv,
    const comskip::localization::Translator& translator);

// Time formatting returns owned values so callers can safely retain results.
std::string dblSecondsToStrMinutes(double seconds);
std::string dblSecondsToStrMinutesFrames(double seconds, double fps);
