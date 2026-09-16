#pragma once

struct RecordingContext;

// Grow and initialize the recording-owned detector buffers.
void InitializeFrameArray(RecordingContext& context, long i);
void InitializeBlackArray(RecordingContext& context, long i);
void InitializeSchangeArray(RecordingContext& context, long i);
void InitializeLogoBlockArray(RecordingContext& context, long i);
void InitializeARBlockArray(RecordingContext& context, long i);
void InitializeACBlockArray(RecordingContext& context, long i);
void InitializeBlockArray(RecordingContext& context, long i);
void InitializeCCBlockArray(RecordingContext& context, long i);
void InitializeCCTextArray(RecordingContext& context, long i);

