#pragma once

#include "platform/file_resources.h"

struct RecordingContext;

void ProcessCSV(RecordingContext& context, comskip::platform::FilePtr input);
