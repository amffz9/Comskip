#pragma once

#include <string>

struct RecordingContext;

// Caption and XDS observations used by commercial detection.
// Subtitle decoding and file output have separate media interfaces.
void OutputCCBlock(RecordingContext &context, long i);
void ProcessCCData(RecordingContext &context);
bool CheckOddParity(unsigned char ch);
void AddNewCCBlock(RecordingContext &context, long current_frame, int type,
                   bool cc_on_screen, bool cc_in_memory);
[[nodiscard]] std::string CCTypeText(RecordingContext &context, int type);
int DetermineCCTypeForBlock(RecordingContext &context, long start, long end);
bool ProcessCCDict(RecordingContext &context);
void Init_XDS_block(RecordingContext &context);
void Add_XDS_block(RecordingContext &context);
void AddXDS(RecordingContext &context, unsigned char hi, unsigned char lo);
void AddCC(RecordingContext &context, int i);
