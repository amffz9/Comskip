#pragma once

struct RecordingContext;

// Runs the optional interactive review session and returns whether to reload.
bool ReviewResult(RecordingContext& context);
void OutputDebugWindow(RecordingContext& context, bool show_video, int frame,
                       int graph_frame, bool force_refresh);
