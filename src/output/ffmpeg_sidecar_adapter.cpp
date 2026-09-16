#include "output/ffmpeg_sidecar_adapter.h"
#include "diagnostic.h"
#include "output/ffmpeg_sidecars.h"
#include "output/output_file.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include <sstream>
#include <vector>

void WriteFfmpegSidecarFiles(RecordingContext& context, bool use_reference) {
    using namespace comskip::output;
    if (!context.settings.output_ffmeta && !context.settings.output_ffsplit) return;
    const int count = use_reference ? context.state.reffer_count : context.state.commercial_count;
    if (use_reference) comskip::detection::validate_intervals(context.state.reffer, count);
    else comskip::detection::validate_intervals(context.state.commercial, count);
    if (context.state.frame_count < 2) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_ffmpeg_sidecar_frame_count);
    std::vector<SidecarChapter> chapters;
    std::vector<SidecarShowSegment> segments;
    const auto time = [&](long frame) { return SidecarSeconds{get_frame_pts(context, frame)}; };
    const auto append = [&](int index, long previous, long start, long end) {
        if (previous != -1 && previous < start) {
            chapters.push_back({time(previous + 1), time(start), SidecarSegmentKind::show});
            segments.push_back({time(previous + 1), time(start), index});
        } else if (previous == -1 && start > 5) {
            chapters.push_back({SidecarSeconds{0}, time(start), SidecarSegmentKind::show});
            segments.push_back({SidecarSeconds{0}, time(start), index});
        }
        const long metadata_start = start <= 5 ? 0 : start;
        if (end - metadata_start > 2)
            chapters.push_back({time(metadata_start), time(end), SidecarSegmentKind::commercial});
    };
    long previous = -1;
    for (int i = 0; i <= count; ++i) {
        const auto start = use_reference ? context.state.reffer[i].start_frame : context.state.commercial[i].start_frame;
        const auto end = use_reference ? context.state.reffer[i].end_frame : context.state.commercial[i].end_frame;
        if (start < 0 || end < start || start <= previous || start >= context.state.frame_count || end > context.state.frame_count)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_ffmpeg_sidecar_commercial_range);
        append(i, previous, start, end);
        previous = end;
    }
    if (count < 0 || previous < context.state.frame_count - 2)
        append(count + 1, previous, context.state.frame_count - 2, context.state.frame_count - 1);
    const auto write = [&](const char* extension, auto serializer, const auto& records) {
        std::ostringstream buffer;
        serializer(buffer, records);
        const auto filename = context.state.outbasename + extension;
        write_output_file(filename, buffer.str());
    };
    if (context.settings.output_ffmeta) write(".ffmeta", write_ffmetadata, std::span<const SidecarChapter>{chapters});
    if (context.settings.output_ffsplit) write(".ffsplit", write_ffsplit, std::span<const SidecarShowSegment>{segments});
}
