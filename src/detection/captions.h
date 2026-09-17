#pragma once

struct RecordingContext;

bool CheckOddParity(unsigned char value);
void Init_XDS_block(RecordingContext& context);
void Add_XDS_block(RecordingContext& context);
void ProcessCCData(RecordingContext& context);
void AddNewCCBlock(RecordingContext& context, long current_frame, int type,
                   bool cc_on_screen, bool cc_in_memory);
int DetermineCCTypeForBlock(RecordingContext& context, long start, long end);
void SetARofBlocks(RecordingContext& context);
bool ProcessCCDict(RecordingContext& context);
