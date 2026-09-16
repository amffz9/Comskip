#include "diagnostic.h"
#include "output/frame_scripts.h"
#include <format>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace comskip::output {
namespace {
void validate(std::span<const ScriptFrameRange> ranges) {
    for (const auto& range : ranges)
        if (range.start < 0 || range.end < range.start)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_retained_script_frame_range);
}
void finish(std::ostream& output) {
    if (!output) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_write_frame_script_output);
}
}
void write_vcf(std::ostream& output, std::span<const VcfRange> ranges) {
    for (const auto& range : ranges)
        if (range.start < 0 || range.length < 0 ||
            range.start > std::numeric_limits<std::int64_t>::max() - range.length)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_virtualdub_subset_range);
    output << "VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n";
    for (const auto& range : ranges)
        output << std::format("VirtualDub.subset.AddRange({},{});\n", range.start, range.length);
    finish(output);
}
void write_projectx(std::ostream& output, std::span<const ScriptFrameRange> ranges) {
    validate(ranges);
    output << "CollectionPanel.CutMode=2\n";
    for (const auto& range : ranges) output << std::format("{}\n{}\n", range.start, range.end);
    finish(output);
}
void write_avisynth(std::ostream& output, std::string_view source_header,
                    std::span<const ScriptFrameRange> trims) {
    validate(trims);
    output << source_header;
    bool first = true;
    for (const auto& trim : trims) {
        output << std::format("{}trim({},{})", first ? "" : " ++ ", trim.start, trim.end);
        first = false;
    }
    output << '\n';
    finish(output);
}
}
