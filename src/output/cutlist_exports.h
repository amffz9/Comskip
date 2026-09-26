#pragma once

#include "detection/detector_records.h"
#include <optional>

struct RecordingContext;

// Export detected intervals through the requested cutlist formats.
bool OutputBlocks(RecordingContext &context);
void ApplyCommercialPadding(RecordingContext &context);
// last_frame is the final frame count, or the frames available so far in live mode.
std::optional<Legacy_commercial_entry> PadCommercialInterval(RecordingContext &context,
                                                             Legacy_commercial_entry interval,
                                                             long last_frame);
void WriteLiveCommercialRecords(RecordingContext &context, const Legacy_commercial_entry &interval);
void OutputTraining(RecordingContext &context);
void OutputStrict(RecordingContext &context, double len, double delta,
                  double tol);
void OpenOutputFiles(RecordingContext &context);
void OutputCommercialBlock(RecordingContext &context, int i, long prev,
                           long start, long end, bool last);
void BuildCommercial(RecordingContext &context);
