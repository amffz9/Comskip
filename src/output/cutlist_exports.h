#pragma once

struct RecordingContext;

// Export detected intervals through the requested cutlist formats.
bool OutputBlocks(RecordingContext &context);
void OutputTraining(RecordingContext &context);
void OutputStrict(RecordingContext &context, double len, double delta,
                  double tol);
void OpenOutputFiles(RecordingContext &context);
void OutputCommercialBlock(RecordingContext &context, int i, long prev,
                           long start, long end, bool last);
void BuildCommercial(RecordingContext &context);
