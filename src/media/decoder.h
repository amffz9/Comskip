#pragma once
#include <cstdio>

struct RecordingContext;
struct VideoState;
struct AVFrame;
struct AVStream;
struct AVPacket;
namespace comskip::localization { class Translator; }

int stream_component_open(RecordingContext& context, VideoState* video, int stream_index);
void file_open(RecordingContext& context);
void file_close(RecordingContext& context);
void DoSeekRequest(RecordingContext& context, VideoState* video);
int video_packet_process(RecordingContext& context, VideoState* video, AVPacket* packet);
double print_decode_progress(RecordingContext& context, int final);
int SubmitFrame(RecordingContext& context, AVStream* stream, AVFrame* frame, double pts);
void Set_seek(RecordingContext& context, VideoState* video, double pts);
void DecodeOnePicture(RecordingContext& context, FILE* file, double pts);
void list_codecs(const comskip::localization::Translator& translator);
