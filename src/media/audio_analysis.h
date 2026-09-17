#pragma once
struct RecordingContext;
struct VideoState;
struct AVFrame;
struct AVPacket;

void backfill_frame_volumes(RecordingContext& context);
void sound_to_frames(RecordingContext& context, VideoState& video, const AVFrame& frame);
void audio_packet_process(RecordingContext& context, VideoState& video, AVPacket& packet);
