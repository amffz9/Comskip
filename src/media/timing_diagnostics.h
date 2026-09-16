#pragma once
struct RecordingContext;

namespace comskip::media {
// Optional CSV diagnostics are owned by the recording; opening writes a header.
bool open_timing_diagnostics(RecordingContext& context);
void write_timing_row(RecordingContext& context, const char* type, double real_pts,
                      double step, double pts, double clock, double offset, int repeat);
void close_timing_diagnostics(RecordingContext& context) noexcept;
}
