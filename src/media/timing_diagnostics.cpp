#include "timing_diagnostics.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include <cstdio>

namespace comskip::media {
namespace {
void write_timing_header(RecordingContext& context) {
    if (context.state.timing_file)
        std::fprintf(context.state.timing_file.get(),
            "sep=,\ntype   ,real_pts, step        ,pts         ,clock       ,delta       ,offset, repeat\n");
}
}
bool open_timing_diagnostics(RecordingContext& context) {
    if (!context.settings.output_timing) return false;
    const auto filename = context.state.inbasename + ".timing.csv";
    context.state.timing_file.reset(myfopen(filename.c_str(), "w"));
    write_timing_header(context);
    return static_cast<bool>(context.state.timing_file);
}
void write_timing_row(RecordingContext& context, const char* type, double real_pts,
                      double step, double pts, double clock, double offset, int repeat) {
    if (context.state.timing_file && !context.state.csStepping &&
        !context.state.csJumping && !context.state.csStartJump)
        std::fprintf(context.state.timing_file.get(),
            "%7s, %12.3f, %12.3f, %12.3f, %12.3f, %12.3f, %12.3f, %d\n",
            type, real_pts, step, pts, clock, pts - clock, offset, repeat);
}
void close_timing_diagnostics(RecordingContext& context) noexcept {
    context.state.timing_file.reset();
}
}
