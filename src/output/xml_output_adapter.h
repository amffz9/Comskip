#pragma once

struct RecordingContext;

// Complete XML export from the selected detector/review list. The reference
// list is selected when review exports its manually maintained marks.
void WriteXmlOutputFiles(RecordingContext& context, bool use_reference = false);
