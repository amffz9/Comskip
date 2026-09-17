#pragma once

struct RecordingContext;

// Rebuild the per-recording logo scan geometry after dimensions or mask bounds change.
void InitScanLines(RecordingContext& context);
void InitHasLogo(RecordingContext& context);
