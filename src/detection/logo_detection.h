#pragma once
#include <span>

struct RecordingContext;

bool CheckFramesForLogo(RecordingContext& context, int start, int end);
bool CheckFrameForLogo(RecordingContext& context, int frame);
double CalculateLogoFraction(RecordingContext& context, int start, int end);
void LoadLogoMaskData(RecordingContext& context);
void SaveLogoMaskData(RecordingContext& context);
void InitLogoBuffers(RecordingContext& context);
void ResetLogoBuffers(RecordingContext& context);
void InitProcessLogoTest(RecordingContext& context);
bool ProcessLogoTest(RecordingContext& context, int frame, int logo_test, int close);
void FillLogoBuffer(RecordingContext& context);
bool SearchForLogoEdges(RecordingContext& context);
double CheckStationLogoEdge(RecordingContext& context, std::span<const unsigned char> frame);
void EdgeDetect(RecordingContext& context, std::span<const unsigned char> frame, int mask_number);
int ClearEdgeMaskArea(RecordingContext& context, unsigned char* temporary,
                      unsigned char* test);
void SetEdgeMaskArea(RecordingContext& context, unsigned char* temporary);
int CountEdgePixels(RecordingContext& context);
void DumpEdgeMask(RecordingContext& context, unsigned char* buffer, int direction);
void DumpEdgeMasks(RecordingContext& context);
void PrintLogoFrameGroups(RecordingContext& context);
void PrintCCBlocks(RecordingContext& context);
char CheckFramesForCommercial(RecordingContext& context, int start, int end);
char CheckFramesForReffer(RecordingContext& context, int start, int end);
