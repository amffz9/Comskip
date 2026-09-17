#pragma once

struct RecordingContext;

bool CheckFramesForLogo(RecordingContext& context, int start, int end);
void LoadLogoMaskData(RecordingContext& context);
void SaveLogoMaskData(RecordingContext& context);
void InitLogoBuffers(RecordingContext& context);
