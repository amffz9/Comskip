#pragma once

struct RecordingContext;

// Rebuilds detector intervals from the per-frame observations.
bool BuildBlocks(RecordingContext& context, bool recalculate);
void CleanLogoBlocks(RecordingContext& context);
