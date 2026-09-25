#pragma once

#include <cstdint>
#include <span>
#include <string_view>

struct RecordingContext;

// Recording diagnostics, histogram thresholds, and reference interval input.
void FindIniFile(RecordingContext& context);
double FindScoreThreshold(RecordingContext& context, double percentile);
void OutputLogoHistogram(RecordingContext& context,
                         std::span<const std::uint64_t> histogram,
                         std::uint64_t denominator);
void OutputbrightHistogram(RecordingContext& context);
void OutputuniformHistogram(RecordingContext& context);
void OutputHistogram(RecordingContext& context, std::span<const int> histogram, int scale,
                     std::string_view title, bool truncate);
int FindBlackThreshold(RecordingContext& context, double percentile);
int FindUniformThreshold(RecordingContext& context, double percentile);
void OutputFrame(RecordingContext& context, int frame_number);
int FindFrameWithPts(RecordingContext& context, double t);
int InputReffer(RecordingContext& context, std::string_view extension, int setfps);
void OutputAspect(RecordingContext& context);
void OutputFrameArray(RecordingContext& context, bool screenOnly);
