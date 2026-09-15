#include "exit_requested.h"
#include "legacy_detection.h"

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
            Debug(1, "Frame Rate set to %5.3f f/s\n", fps);
            if (ticks > 1)
                Debug(1, "Repeats per frame = %d\n", ticks);
            if ((fabs(fps - dfps) > 0.1)) {
                Debug(1, "DFps[%d]= %5.3f f/s\n", ticks, dfps);
            }
            if (fabs(fps - rfps) > 0.1) {
                Debug(1, "RFps[%d]= %5.3f f/s\n", ticks, rfps);
            }
            if (fabs(fps - afps) > 0.1) {
                Debug(1, "AFps[%d]= %5.3f f/s\n", ticks, afps);
            }
#endif
            if ( new_fps > 9.0 && new_fps < 150 && fabs(new_fps - context.settings.fps) > 1. )
            {
                context.settings.fps = new_fps;
                Debug(context, 1, "Frame Rate set to %5.3f f/s\n", context.settings.fps);
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
/* no longer used

#define MAX_SAVED_VOLUMES	10000

static struct
{
    int frame;
    int volume;
} volumes[MAX_SAVED_VOLUMES];
static int max_fill = 0;

void SaveVolume (int f,int v)
{
    int i;
    for (i = 0; i < MAX_SAVED_VOLUMES; i++)
    {
        if (volumes[i].frame ==0)
        {
            volumes[i].frame = f;
            volumes[i].volume = v;
            if (i > max_fill)
                max_fill = i;
            return;
        }
    }
    Debug (1, "Panic volume buffer\n");
    if (f > 8 * 60 * 60 * 50)  // max 8 hours with fps of 50
    {
        Debug(0, "Too many volume panic's, protected file?\n");
        comskip::request_exit(103);   // exit as probably protected file .
    }

    for (i = 0; i < MAX_SAVED_VOLUMES; i++)
    {
        volumes[i].frame = 0;
    }
    max_fill = 0;
}

int RetreiveVolume (int f)
{
    int i;
    for (i = 0; i <= max_fill; i++)
    {
        if (volumes[i].frame ==f)
        {
            volumes[i].frame = 0;
            return(volumes[i].volume);
        }
    }
    return(-1);
}


void ClearVolumeBuffer ()
{
    int i;
    for (i = 0; i <= max_fill; i++)
    {
        volumes[i].frame = 0;
        volumes[i].volume = 0;
    }
    max_fill = 0;
}
*/

void set_frame_volume(RecordingContext& context, unsigned int f, int volume)
{
    int i;
    int act_framenum;
    if (!context.state.initialized) return;

//	ascr += 1;
    act_framenum = f;

    if (act_framenum > 0)
    {
        if (context.state.framearray)
            if (act_framenum <= context.state.frame_count)
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
/*
        if (act_framenum > frame_count) {
            SaveVolume(act_framenum, volume);
            if (act_framenum  > frame_count + 10000) // too many audio frames without video
            {
                Debug(0, "Too much audio without video, protected file or bug?\n");
                comskip::request_exit(103);   // exit as probably protected file .
            }
        }
*/
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



