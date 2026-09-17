#include "timing_diagnostics.h"
#include "recording_context.h"
#include "platform/platform.h"
#include "platform/utf8_paths.h"
#include "output/checked_file.h"
#include <cstdio>
#include <string>

namespace comskip::media {
namespace {
void write_timing_header(RecordingContext& context) {
    if (context.state.timing_file)
        comskip::output::checked_fprintf(*context.state.timing_file, context.state.inbasename + ".timing.csv",
            "sep=,\ntype   ,real_pts, step        ,pts         ,clock       ,delta       ,offset, repeat\n");
}
}
bool open_timing_diagnostics(RecordingContext& context) {
    if (!context.settings.output_timing) return false;
    const auto filename = context.state.inbasename + ".timing.csv";
    context.state.timing_file = comskip::platform::open_file_owned(filename, "w");
    write_timing_header(context);
    return static_cast<bool>(context.state.timing_file);
}
void write_timing_row(RecordingContext& context, std::string_view type, double real_pts,
                      double step, double pts, double clock, double offset, int repeat) {
    if (context.state.timing_file && !context.state.csStepping &&
        !context.state.csJumping && !context.state.csStartJump)
        comskip::output::checked_fprintf(*context.state.timing_file, context.state.inbasename + ".timing.csv",
            "%7s, %12.3f, %12.3f, %12.3f, %12.3f, %12.3f, %12.3f, %d\n",
            std::string(type).c_str(), real_pts, step, pts, clock, pts - clock, offset, repeat);
}
void close_timing_diagnostics(RecordingContext& context) noexcept {
    context.state.timing_file.reset();
}
}
