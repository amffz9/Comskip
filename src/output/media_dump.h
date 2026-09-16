#pragma once

struct RecordingContext;

// Optional binary media and caption-observation dumps owned by the recording.
void dump_audio_start(RecordingContext& context);
void dump_audio(RecordingContext& context, char* start, char* end);
void dump_video_start(RecordingContext& context);
void dump_video(RecordingContext& context, char* start, char* end);
void close_dump(RecordingContext& context);
void dump_data(RecordingContext& context, char* start, int length);
void close_data(RecordingContext& context);
