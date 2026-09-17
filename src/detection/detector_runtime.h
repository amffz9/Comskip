#pragma once

struct RecordingContext;

// Detector pipeline operations shared by decoded and CSV-backed recordings.
void InsertBlackFrame(RecordingContext& context, int frame, int brightness,
                      int uniformity, int volume, int cause);
bool BuildMasterCommList(RecordingContext& context);
void BuildCommListAsYouGo(RecordingContext& context);
void ProcessARInfoInit(RecordingContext& context, int min_y, int max_y,
                       int min_x, int max_x);
void ProcessARInfo(RecordingContext& context, int min_y, int max_y,
                   int min_x, int max_x);
void ProcessACInfoInit(RecordingContext& context, int audio_channels);
void ProcessACInfo(RecordingContext& context, int audio_channels);
bool ProcessLogoTest(RecordingContext& context, int frame_number,
                     int current_logo_test, int close);