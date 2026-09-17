#include "exit_requested.h"
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
//    double old_fps = fps;
    double new_fps = (double)1.0 / fp;
//    static int showed_fps=0;
 #ifdef notused

    static int fps_correction_count = 0;
    if (fabs(old_fps-new_fps) > 0.01 /* && showed_fps++ < 4 */ ) {
        if (fps_correction_count++ > 4 || old_fps == 1) {
            fps = new_fps;
            if (fps != old_fps)
                showed_fps=0.0;
            Debug(context, 1, "%s", context.translator.format("media_frame_rate_set", std::format("{:5.3f}", fps)).c_str());
            if (ticks > 1)
                Debug(context, 1, "%s", context.translator.format("media_repeats_per_frame", ticks).c_str());
            if ((fabs(fps - dfps) > 0.1)) {
                Debug(context, 1, "%s", context.translator.format("media_dfps", ticks, std::format("{:5.3f}", dfps)).c_str());
            }
            if (fabs(fps - rfps) > 0.1) {
                Debug(context, 1, "%s", context.translator.format("media_rfps", ticks, std::format("{:5.3f}", rfps)).c_str());
            }
            if (fabs(fps - afps) > 0.1) {
                Debug(context, 1, "%s", context.translator.format("media_afps", ticks, std::format("{:5.3f}", afps)).c_str());
            }
#endif
            if ( new_fps > 9.0 && new_fps < 150 && fabs(new_fps - context.settings.fps) > 1. )
            {
                context.settings.fps = new_fps;
                Debug(context, 1, "%s", context.translator.format("media_frame_rate_set", std::format("{:5.3f}", context.settings.fps)).c_str());
 //               if (/* old_fps != fps && */ showed_fps < 4)
//                    Debug(1, "Frame Rate corrected to %5.3f f/s\n", fps);
            }
/*
        }

    }
    else
        fps_correction_count = 0;
*/

}
void set_frame_volume(RecordingContext& context, unsigned int f, int volume)
{
    int i;
    int act_framenum;
    if (!context.state.initialized) return;
    if (f > static_cast<unsigned int>(std::numeric_limits<int>::max())) return;

//	ascr += 1;
    act_framenum = f;

    if (act_framenum > 0)
    {
        if (context.state.framearray)
            if (act_framenum <= context.state.frame_count &&
                static_cast<std::size_t>(act_framenum) < context.state.frame.size())
            {
 //               Debug(1, "Audio running after video\n");
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
//	audio_framenum++;
//	ascr += 1;
}




