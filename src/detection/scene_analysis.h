#pragma once

struct RecordingContext;

void LoadCutScene(RecordingContext& context, const char* filename);
void RecordCutScene(RecordingContext& context, int frame_count, int brightness);
int CountSceneChanges(RecordingContext& context, int start_frame, int end_frame);
