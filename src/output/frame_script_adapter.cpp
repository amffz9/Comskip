#include "diagnostic.h"
#include "output/frame_script_adapter.h"
#include "output/frame_scripts.h"
#include "output/output_file.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include "checked_format.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <sstream>
#include <optional>
#include <vector>

void WriteFrameScriptFiles(RecordingContext& context, bool use_reference) {
    using namespace comskip::output;
    const auto& state = context.state;
    const auto& settings = context.settings;
    if (!settings.output_vcf && !settings.output_projectx && !settings.output_avisynth) return;
    const int count = use_reference ? state.reffer_count : state.commercial_count;
    if (use_reference) comskip::detection::validate_intervals(state.reffer, count);
    else comskip::detection::validate_intervals(state.commercial, count);
    if (!std::isfinite(settings.fps) || settings.fps <= 0 || state.frame_count < 2)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_frame_script_media_geometry);
    if (!state.frame.empty() && (state.frame.size() < 2 || state.framenum_real < 2 ||
        static_cast<std::size_t>(state.framenum_real) > state.frame.size()))
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::frame_script_timestamps_exceed_frame_buffer);
    // Preserve F2F's real decoder-count clamp and +1.5 truncation, which differs
    // from the timestamp policy used for time-based sidecars.
    const auto position = [&](long frame) {
        const double time = state.frame.empty() ? static_cast<double>(frame) / settings.fps :
            state.frame[frame <= 0 ? 1 : std::min<long>(frame, state.framenum_real - 1)].pts;
        const double result = time * settings.fps + 1.5;
        if (!std::isfinite(result) || result < 0 || result >= std::ldexp(1.0,63))
            throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::frame_script_position_exceeds_integer_range);
        return static_cast<std::int64_t>(result);
    };
    std::vector<VcfRange> vcf;
    std::vector<ScriptFrameRange> retained;
    const auto append = [&](std::optional<long> previous, long start) {
        if (!previous || *previous < start) {
            retained.push_back({position(previous ? *previous + 1 : 0), position(start)});
            if (previous && start - *previous > 5 && *previous > 0)
                vcf.push_back({position(*previous - 1), position(start) - position(*previous)});
        }
    };
    std::optional<long> previous;
    for (int i = 0; i <= count; ++i) {
        const auto start = use_reference ? state.reffer[i].start_frame : state.commercial[i].start_frame;
        const auto end = use_reference ? state.reffer[i].end_frame : state.commercial[i].end_frame;
        if (start < 0 || end < start || (previous && start <= *previous) || start >= state.frame_count || end > state.frame_count)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_frame_script_commercial_range);
        append(previous, start); previous = end;
    }
    if (count < 0 || !previous || *previous < state.frame_count - 2) append(previous, state.frame_count - 2);
    const auto write = [&](const std::string& filename, auto serialize) {
        std::ostringstream contents; serialize(contents);
        write_output_file(filename, contents.str());
    };
    if (settings.output_vcf) write(state.outbasename + ".vcf",[&](auto& out) { write_vcf(out,vcf); });
    if (settings.output_projectx) write(state.mpegfilename + ".Xcl",[&](auto& out) { write_projectx(out,retained); });
    if (settings.output_avisynth) {
        std::string header;
        if (settings.avisynth_options.empty())
            header = std::format("LoadPlugin(\"MPEG2Dec3.dll\") \nMPEG2Source(\"{}\")\n",state.mpegfilename);
        else comskip::checked_format(header,settings.avisynth_options.c_str(),state.mpegfilename.c_str());
        write(state.mpegfilename + ".avs",[&](auto& out) { write_avisynth(out,header,retained); });
    }
}
