#pragma once

struct RecordingContext;

// Calculates final commercial scores from the detector intervals.
void WeighBlocks(RecordingContext& context);
void BuildPunish(RecordingContext& context);
