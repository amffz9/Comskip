#pragma once
#include <cstdio>

struct RecordingContext;
struct VideoState;
struct AVFrame;
struct AVStream;
namespace comskip::localization { class Translator; }

void file_open(RecordingContext& context);
void file_close(RecordingContext& context);
int SubmitFrame(RecordingContext& context, AVStream* stream, AVFrame* frame, double pts);
void Set_seek(RecordingContext& context, VideoState* video, double pts);
void DecodeOnePicture(RecordingContext& context, FILE* file, double pts);
void list_codecs(const comskip::localization::Translator& translator);
