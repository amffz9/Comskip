#include "decode_progress.h"
#include "decoder.h"
#include "detection/frame_timestamps.h"
#include "recording_context.h"
#include <cstdio>
#include <thread>

double print_decode_progress(RecordingContext& context, int final) {
    auto& progress = context.state.decode_progress;
    if (context.state.decoder_verbose || context.state.csStepping) return 0;
    if (final < 0) { progress.reset(); return 0; }
    if (final) {
        const auto summary = progress.snapshot();
        fputs(context.translator.format("media_decoded_summary", summary.frames,
            std::format("{:.2f}", summary.elapsed.count()),
            std::format("{:.2f}", summary.average_fps())).c_str(), stderr);
        fflush(stderr);
        return summary.average_fps();
    }
    progress.observe_frame();
#if !defined(DONATOR) && !defined(DEBUG)
    // The public-build speed policy waits without counting the same frame again.
    while (context.state.is_h264 && progress.snapshot().interval_frames > 15 &&
           progress.snapshot().interval < std::chrono::seconds(1))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    const auto report = progress.report();
    if (!report) return 0;
    const double fps = get_fps(context);
    const double seconds = std::isfinite(fps) && fps > 0 ? context.state.framenum / fps : 0;
    fputs(context.translator.format("media_decode_progress", comskip::media::decode_position(seconds), report->frames,
        std::format("{:.2f}", report->elapsed.count()), std::format("{:.2f}", report->average_fps()),
        std::format("{:.2f}", report->interval.count()), std::format("{:.2f}", report->interval_fps()),
        comskip::media::decode_completion_percent(seconds, context.state.video_owner->duration)).c_str(), stderr);
    fputc('\r', stderr);
    fflush(stderr);
    return report->average_fps();
}
