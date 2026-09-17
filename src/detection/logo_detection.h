#pragma once

struct RecordingContext;

bool CheckFramesForLogo(RecordingContext& context, int start, int end);
void LoadLogoMaskData(RecordingContext& context);
void SaveLogoMaskData(RecordingContext& context);
void InitLogoBuffers(RecordingContext& context);
void PrintLogoFrameGroups(RecordingContext& context);
char CheckFramesForCommercial(RecordingContext& context, int start, int end);
char CheckFramesForReffer(RecordingContext& context, int start, int end);
