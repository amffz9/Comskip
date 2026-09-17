#pragma once

#include "platform/file_resources.h"

struct RecordingContext;

comskip::platform::FilePtr reopen_csv_inputs(RecordingContext& context);
void ProcessCSV(RecordingContext& context, comskip::platform::FilePtr input);
