#include "app/debug.h"
#include "app/recording_context.h"
#include "localization/diagnostic.h"
#include <cmath>
#include <format>
#include <limits>

double get_fps(RecordingContext& context)
{
    return context.settings.fps;
}


void set_fps(RecordingContext& context, double fp)
{
    if (!std::isfinite(fp) || fp <= 0.0) return;
    const double new_fps = 1.0 / fp;
    if (new_fps > 9.0 && new_fps < 150 && std::fabs(new_fps - context.settings.fps) > 1.)
    {
        context.settings.fps = new_fps;
        Debug(context, 1, context.translator.format("media_frame_rate_set", std::format("{:5.3f}", context.settings.fps)));
    }
}

void set_frame_volume(RecordingContext& context, unsigned int f, int volume)
{
    int i;
    int act_framenum;
    if (!context.state.initialized) return;
    if (f > static_cast<unsigned int>(std::numeric_limits<int>::max())) return;

    act_framenum = f;

    if (act_framenum > 0)
    {
        if (context.state.framearray)
            if (act_framenum <= context.state.frame_count &&
                static_cast<std::size_t>(act_framenum) < context.state.frame.size())
            {
                if (context.state.frame[act_framenum].brightness > 5)
                    context.state.frame[act_framenum].volume = volume;
                if (volume >= 0)
                {
                    context.state.volumeHistogram[(volume/context.state.volumeScale < 255 ? volume/context.state.volumeScale : 255)]++;
                    context.state.silenceHistogram[(volume < 255 ? volume : 255)]++;
                }
            }

        i = context.state.black_count-1;
        while (i > 0 && context.state.black[i].frame > act_framenum)
            i--;
        if ( i >= 0 && context.state.black[i].frame == act_framenum )
            if (context.state.black[i].brightness > 0) context.state.black[i].volume = volume;
        // Set the zero above to 5 if you do not want the volume to be updated for uniform frames etc.
    }
}


