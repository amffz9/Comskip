#include "output/evidence_adapter.h"
#include "output/evidence.h"
#include "output/output_file.h"
#include "recording_context.h"
#include "app/debug.h"
#include "detection/detection_methods.h"
#include "detection/frame_timestamps.h"
#include "detection/logo_detection.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <sstream>

void WriteEvidenceFile(RecordingContext& context, bool final) {
    using namespace comskip::output;
    if (!context.settings.output_evidence) return;
    auto& state = context.state;
    const FrameTime time = [&](long frame) { return get_frame_pts(context, static_cast<int>(frame)); };
    Evidence evidence;
    evidence.revision = ++state.evidence_revision;
    evidence.final = final;
    evidence.logo_known = state.logoInfoAvailable && comskip::detection::method_enabled(
        context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo);
    // The final cut list adds a cut at the last frame processed; it is no seam.
    evidence.seams = evidence_seams(std::span(state.black).first(static_cast<std::size_t>(
        std::clamp<long>(state.black_count, 0, static_cast<long>(state.black.size())))),
        final ? state.framesprocessed : std::numeric_limits<long>::max(), time);
    if (evidence.logo_known && state.frame.size() > 1) {
        const auto last = std::clamp<long>(state.frame_count, 0, static_cast<long>(state.frame.size()) - 1);
        evidence.logo = evidence_logo(std::span(state.frame).subspan(1, static_cast<std::size_t>(last)), 1, time);
    }
    if (final) {
        for (long i = 0; i < state.block_count; ++i) {
            const auto& block = state.cblock[i];
            // The same test the log's weighted block list prints as '-'.
            const bool commercial = CheckFramesForCommercial(context, block.f_start + block.b_head,
                                                             block.f_end - block.b_tail) == '-';
            evidence.blocks.push_back({time(block.f_start), time(block.f_end), block.score,
                                       commercial ? BlockDecision::commercial : BlockDecision::programme});
        }
    } else {
        for (long i = 0; i <= state.commercial_count; ++i)
            evidence.blocks.push_back({time(state.commercial[i].start_frame), time(state.commercial[i].end_frame),
                                       std::nullopt, BlockDecision::commercial});
    }
    std::ostringstream serialized;
    write_evidence(serialized, evidence);
    const auto filename = state.outbasename + ".evidence.json";
    if (final) {
        replace_output_file(filename, serialized.str(), std::chrono::milliseconds{50});
        return;
    }
    // A live rewrite that cannot replace the file (a Windows reader holding it)
    // keeps the previous whole file; the next rewrite carries a higher revision.
    try {
        replace_output_file(filename, serialized.str(), std::chrono::milliseconds{50});
    } catch (const std::ios_base::failure& error) {
        Debug(context, 1, std::string(error.what()) + "\n");
    }
}
