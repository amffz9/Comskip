#pragma once
#include <cstdio>
#include "video_packet_outcome.h"

struct RecordingContext;
struct VideoState;
struct AVFrame;
struct AVStream;
struct AVPacket;
namespace comskip::localization { class Translator; }

namespace comskip::media { enum class StreamOpenResult { opened, unavailable }; }
namespace comskip::media { enum class DecodeProgressMode { observe, finalize, reset }; }
[[nodiscard]] comskip::media::StreamOpenResult stream_component_open(RecordingContext& context, VideoState& video, int stream_index);
void file_open(RecordingContext& context);
void file_close(RecordingContext& context);
void DoSeekRequest(RecordingContext& context, VideoState& video);
comskip::media::VideoPacketOutcome video_packet_process(RecordingContext& context, VideoState& video, AVPacket* packet);
double print_decode_progress(RecordingContext& context, comskip::media::DecodeProgressMode mode);
[[nodiscard]] comskip::media::FrameSubmission SubmitFrame(RecordingContext& context, AVFrame& frame, double pts);
void Set_seek(RecordingContext& context, VideoState& video, double pts);
[[nodiscard]] comskip::media::VideoPacketOutcome DecodeOnePicture(RecordingContext& context, double pts);
void list_codecs(const comskip::localization::Translator& translator);
