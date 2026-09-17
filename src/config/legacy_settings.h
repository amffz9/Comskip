#pragma once

#include <cstdio>

struct RecordingContext;
namespace comskip::localization { class Translator; }

// Legacy configuration reload entry point used by the review workflow.
void LoadIniFile(RecordingContext& context);
void LoadIniFile(RecordingContext& context, const comskip::localization::Translator& translator);
FILE* LoadSettings(RecordingContext& context, int argc, char** argv,
                   const comskip::localization::Translator& translator);
