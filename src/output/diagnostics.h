#pragma once

struct RecordingContext;

// Recording diagnostics, histogram thresholds, and reference interval input.
void FindIniFile(RecordingContext& context);
double FindScoreThreshold(RecordingContext& context, double percentile);
void OutputLogoHistogram(RecordingContext& context, int buckets);
void OutputbrightHistogram(RecordingContext& context);
void OutputuniformHistogram(RecordingContext& context);
void OutputHistogram(RecordingContext& context, int* histogram, int scale,
                     char* title, bool truncate);
int FindBlackThreshold(RecordingContext& context, double percentile);
int FindUniformThreshold(RecordingContext& context, double percentile);
void OutputFrame(RecordingContext& context, int frame_number);
int FindFrameWithPts(RecordingContext& context, double t);
int InputReffer(RecordingContext& context, const char* extension, int setfps);
void OutputAspect(RecordingContext& context);
void OutputBlackArray(RecordingContext& context);
void OutputFrameArray(RecordingContext& context, bool screenOnly);
