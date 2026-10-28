#pragma once
struct RecordingContext;
// Replaces <outbasename>.evidence.json when output_evidence is set. The final
// export lists the scored blocks; a live export lists the padded live intervals.
// A failed live replacement is logged and leaves the previous file in place.
void WriteEvidenceFile(RecordingContext& context, bool final);
