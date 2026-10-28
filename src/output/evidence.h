#pragma once
#include "detection/detector_records.h"
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <vector>

namespace comskip::output {
// Seam kinds as the evidence file names them: black, silence, scene, aspect.
enum class SeamKind : unsigned { black = 1u << 0, silence = 1u << 1, scene = 1u << 2, aspect = 1u << 3 };

struct EvidenceSeam { double at{}; unsigned kinds{}; };
struct EvidenceInterval { double from{}; double to{}; };
enum class BlockDecision { commercial, programme };
// Live candidates are not scored, so their score is absent (written as null).
struct EvidenceBlock { double from{}; double to{}; std::optional<double> score; BlockDecision decision{}; };

struct Evidence {
    long long revision{};
    bool final{};
    bool logo_known{};
    std::vector<EvidenceSeam> seams;
    std::vector<EvidenceInterval> logo;
    std::vector<EvidenceBlock> blocks;
};

using FrameTime = std::function<double(long frame)>;

// Groups the detector's cut frames (sorted by frame) whose times are less than a
// second apart, as block building does. Each group is one seam at the midpoint of
// its first and last frame, with the union of its kinds; uniform frames count as
// black. Frames at or after end_frame (the end-of-recording cut) are ignored, as
// are groups with none of the four kinds.
std::vector<EvidenceSeam> evidence_seams(std::span<const black_frame_info> cut_frames,
                                         long end_frame, const FrameTime& time);
// Runs of frames with logo_present; frames[0] is frame first_frame. A run ends at
// the first frame without a logo, or at the last frame.
std::vector<EvidenceInterval> evidence_logo(std::span<const frame_info> frames, long first_frame,
                                            const FrameTime& time);
// Writes one JSON object on one line ending in '\n', times in seconds rounded to
// three decimals and scores to two.
// Throws invalid_argument for a non-finite number and ios_base::failure for a
// failed stream.
void write_evidence(std::ostream& output, const Evidence& evidence);
}
