#pragma once
#include <cstdint>
#include <span>

struct RecordingContext;

// Optional binary media and caption-observation dumps owned by the recording.
void dump_audio_start(RecordingContext& context);
void dump_audio(RecordingContext& context, std::span<const std::uint8_t> data);
void dump_video_start(RecordingContext& context);
void dump_video(RecordingContext& context, std::span<const std::uint8_t> data);
void close_dump(RecordingContext& context);
void dump_data(RecordingContext& context, std::span<const std::uint8_t> data);
void close_data(RecordingContext& context);
