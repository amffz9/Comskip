#pragma once

struct RecordingContext;

void LoadCutScene(RecordingContext& context, const char* filename);
void RecordCutScene(RecordingContext& context, int frame_count, int brightness);
