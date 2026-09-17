#pragma once
#include <string_view>

struct RecordingContext;

void LoadCutScene(RecordingContext& context, std::string_view filename);
void RecordCutScene(RecordingContext& context, int frame_count, int brightness);
int CountSceneChanges(RecordingContext& context, int start_frame, int end_frame);
bool CheckSceneHasChanged(RecordingContext& context);
