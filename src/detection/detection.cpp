#include "../localization/diagnostic.h"
#include "legacy_detection.h"
#include "media/audio_analysis.h"
#include <format>
#include "frame_mask.h"
#include "logo_sampling.h"
#include "logo_shrink.h"
#include "volume_histogram.h"
#include "output/run_log.h"
#include <array>
#include <stdexcept>
#include <utility>

namespace {
template <typename... Args>
void DetectionDebug(RecordingContext& context, int level, const char* key, Args&&... args)
{
    Debug(context, level, "%s", context.translator.format(key, std::forward<Args>(args)...).c_str());
}
}

int DetectCommercials(RecordingContext& context, int f, double pts)
{
    bool isBlack = 0;	/*Gil*/
    int i,j;
    long oldBlack_count;


    if (context.state.loadingTXT)
        return(0);
    if (context.state.loadingCSV)
        return(0);
    if (!context.state.initialized)
        InitComSkip(context);
//	frame_count++;
    context.state.frame_count = context.state.framenum_real = context.state.framenum+1;

//Debug(1, "Frame info f=%d, framenum=%d, framenum_real=%d, frame_count=%d\n",f, framenum, framenum_real, frame_count, max_frame_count);

    context.state.avg_fps = 1.0/ (pts / context.state.frame_count);

    if (context.state.framenum_real < 0) return 0;
    if (context.state.play_nice) sleep_for_ms(context.settings.play_nice_sleep);
    if (context.state.framearray) InitializeFrameArray(context, context.state.framenum_real);
//	curvolume = RetreiveVolume(framenum_real);
    //curvolume = RetreiveVolume(frame_count);

    if (pts < 0.0)
        pts = 0.0;
    context.state.frame[context.state.frame_count].pts = pts;
    context.state.frame[context.state.frame_count].pict_type = context.state.pict_type;
    if (context.state.frame_count == 1)
        context.state.frame[0].pts = pts;
//    curvolume = retreive_frame_volume(get_frame_pts(frame_count-1), get_frame_pts(frame_count));
    context.state.frame[context.state.frame_count].volume = -1;
    backfill_frame_volumes(context);
    context.state.curvolume = context.state.frame[context.state.frame_count].volume;

//	if (frame_count != framenum_real)
//		Debug(0, "Inconsistent frame numbers\n");
    if (context.state.framearray)
    {
        context.state.frame[context.state.frame_count].volume = context.state.curvolume;
        context.state.frame[context.state.frame_count].goppos = context.state.headerpos;

        context.state.frame[context.state.frame_count].cur_segment = context.state.debug_cur_segment;
        context.state.frame[context.state.frame_count].audio_channels = context.state.audio_channels;

    }
    if (context.state.curvolume > 0)
    {
        context.state.volumeHistogram[(context.state.curvolume/context.state.volumeScale < 255 ? context.state.curvolume/context.state.volumeScale : 255)]++;
    }
    const auto mask_storage = comskip::detection::frame_mask_storage_size(
        context.state.videowidth, context.state.height, context.state.width);
    if (!context.state.frame_ptr)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::frame_mask_requires_decoded_image_pixels);
    comskip::detection::apply_frame_mask(
        std::span<unsigned char>(context.state.frame_ptr, mask_storage),
        context.state.videowidth, context.state.height, context.state.width,
        {context.settings.ticker_tape, context.settings.top_ticker_tape,
         context.settings.ticker_tape_percentage, context.settings.top_ticker_tape_percentage,
         context.settings.ignore_side, context.settings.ignore_left_side,
         context.settings.ignore_right_side});
    oldBlack_count = context.state.black_count;	/*Gil*/
    CheckSceneHasChanged(context);
    isBlack = oldBlack_count != context.state.black_count;	/*Gil*/


    if ((context.settings.commDetectMethod & LOGO) &&
        context.state.frame_count % comskip::detection::logo_sampling_interval(context.settings.fps, context.state.logoFreq) == 0)
    {
        if (!context.state.logoInfoAvailable || (!context.state.lastLogoTest && !context.settings.startOverAfterLogoInfoAvail) )
        {
            if (context.settings.delay_logo_search == 0 ||
                    (context.settings.delay_logo_search == 1 && F2T(context.state.frame_count) > context.settings.added_recording * 60) ||
                    (context.settings.delay_logo_search > 1 && F2T(context.state.frame_count) > context.settings.delay_logo_search))
            {
                FillLogoBuffer(context);
                if (context.state.logoBuffersFull)
                {
                    DetectionDebug(context, 6, "detection_logo_search_frames",
                        std::format("{}", context.state.logoFrameNum[context.state.oldestLogoBuffer]),
                        std::format("{}", context.state.frame_count));
                    if(!SearchForLogoEdges(context))
                    {
                        InitComSkip(context);
                        return 1;
                    }
                }
                if (context.state.logoInfoAvailable)
                {
//				logoTrendCounter = num_logo_buffers;
//				lastLogoTest = true;
//				curLogoTest = true;

                    //				logoTrendStartFrame = logoFrameNum[oldestLogoBuffer];
//				curLogoTest = true;
//				lastRealLogoChange = logoFrameNum[oldestLogoBuffer];
//				if (num_logo_buffers >= minHitsForTrend) {
//					hindsightLogoState = true;
//				} else {
//					hindsightLogoState = false;
//				}
                }
            }
        }
        if (context.state.logoInfoAvailable)
        {
//			EdgeCount(frame_ptr);
//			curLogoTest = logoBuffersFull;
            context.state.currentGoodEdge = CheckStationLogoEdge(context, context.state.frame_ptr);
            context.state.curLogoTest = (context.state.currentGoodEdge > context.settings.logo_threshold);
            context.state.lastLogoTest = ProcessLogoTest(context, context.state.frame_count, context.state.curLogoTest, false);
            if (!context.state.lastLogoTest && !context.settings.startOverAfterLogoInfoAvail && context.state.logoBuffersFull)   // Lost logo
            {
//				logoInfoAvailable = false;
//				secondLogoSearch = true;
                context.state.logoBuffersFull = false;
                InitLogoBuffers(context);
                context.state.newestLogoBuffer = -1;
            }
            if (context.settings.startOverAfterLogoInfoAvail && !context.state.loadingCSV && !context.state.secondLogoSearch && context.state.logo_block_count > 0 &&
                    !context.state.lastLogoTest &&
                    F2L(context.state.frame_count,context.state.logo_block[context.state.logo_block_count-1].end) > ( context.settings.max_commercialbreak * 1.2 ) &&
                    (double)context.state.frames_with_logo / (double)context.state.frame_count < 0.5
               )
            {
                DetectionDebug(context, 6, "detection_logo_search_restart",
                    std::format("{}", context.state.logo_block[context.state.logo_block_count-1].end),
                    std::format("{}", context.state.frame_count));
                // First logo found but no logo found after first commercial cblock so search new logo
                context.state.logoInfoAvailable = false;
                context.state.secondLogoSearch = true;
                context.state.logoBuffersFull = false;
                InitLogoBuffers(context);
                context.state.newestLogoBuffer = -1;
            }
        }
    }

//	EdgeCount(frame_ptr);
//	currentGoodEdge = ((double) edge_count) / 750;

    if (context.state.logoInfoAvailable && context.state.framearray) {
      context.state.frame[context.state.frame_count].logo_present = context.state.lastLogoTest;
    } else if (context.state.framearray) {
      context.state.frame[context.state.frame_count].logo_present = 0.0;
    }
    if (context.state.lastLogoTest)
        context.state.frames_with_logo = comskip::detection::add_logo_frames(context.state.frames_with_logo, 1);
    if (context.state.framearray) context.state.frame[context.state.frame_count].currentGoodEdge = context.state.currentGoodEdge;

    if (context.state.frame_count == 1 || ((context.state.frame_count & context.state.subsample_video) == 0))
        OutputDebugWindow(context, true,context.state.frame_count,true, false);
//	key = 0;
//	while (key==0)
//		vo_wait();

    context.state.framesprocessed++;
    context.state.scr += 1;

    if (context.settings.live_tv && !isBlack)
    {
        BuildCommListAsYouGo(context);
    }

    return 0;
}



int Max(int i,int j)
{
    return(i>j?i:j);
}

int Min(int i,int j)
{
    return(i<j?i:j);
}


double AverageARForBlock(RecordingContext& context, int start, int end)
{
    int i, maxSize;
    double Ar;
    int f,t;

    maxSize = 0;
    Ar = 0.0;
    for (i = 0; i < context.state.ar_block_count; i++)
    {
        f = max(context.state.ar_block[i].start, start);
        t = min(context.state.ar_block[i].end, end);
        if (maxSize < t-f+1)
        {
            Ar = context.state.ar_block[i].ar_ratio;
            maxSize = t-f+1;
        }
        if (context.state.ar_block[i].start > end)
            break;
    }
    return(Ar);
}

int AverageACForBlock(RecordingContext& context, int start, int end)
{
    int i, maxSize;
    int Ac;
    int f,t;

    maxSize = 0;
    Ac = 0;
    for (i = 0; i < context.state.ac_block_count; i++)
    {
        f = max(context.state.ac_block[i].start, start);
        t = min(context.state.ac_block[i].end, end);
        if (maxSize < t-f+1)
        {
            Ac = context.state.ac_block[i].audio_channels;
            maxSize = t-f+1;
        }
        if (context.state.ac_block[i].start > end)
            break;
    }
    return(Ac);
}

double	FindARFromHistogram(RecordingContext& context, double ar_ratio)
{
    int i;
    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        if (ar_ratio > context.state.ar_histogram[i].ar_ratio - context.settings.ar_delta &&
                ar_ratio < context.state.ar_histogram[i].ar_ratio + context.settings.ar_delta)
            return (context.state.ar_histogram[i].ar_ratio);
    }
    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        if (ar_ratio > context.state.ar_histogram[i].ar_ratio - 2*context.settings.ar_delta &&
                ar_ratio < context.state.ar_histogram[i].ar_ratio + 2*context.settings.ar_delta)
            return (context.state.ar_histogram[i].ar_ratio);
    }
    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        if (ar_ratio > context.state.ar_histogram[i].ar_ratio - 4*context.settings.ar_delta &&
                ar_ratio < context.state.ar_histogram[i].ar_ratio + 4*context.settings.ar_delta)
            return (context.state.ar_histogram[i].ar_ratio);
    }
    return (0.0);
}

void FillARHistogram(RecordingContext& context, bool refill)
{
    int		i;
    bool	hadToSwap;
    long	tempFrames;
    double	tempRatio;
    long	totalFrames = 0;
    long	tempCount;
    long	counter;
    int		hi;

    if (refill)
    {

        for (i = 0; i < MAX_ASPECT_RATIOS; i++)
        {
            context.state.ar_histogram[i].frames = 0;
            context.state.ar_histogram[i].ar_ratio = 0.0;
        }

        for (i = 0; i < context.state.ar_block_count; i++)
        {
            hi = (int)((context.state.ar_block[i].ar_ratio - 0.5)*100);
            if (hi >= 0 && hi < MAX_ASPECT_RATIOS)
            {
                context.state.ar_histogram[hi].frames += context.state.ar_block[i].end - context.state.ar_block[i].start + 1;
                context.state.ar_histogram[hi].ar_ratio = context.state.ar_block[i].ar_ratio;
            }
        }
    }

    counter = 0;
    do
    {
        hadToSwap = false;
        counter++;
        for (i = 0; i < MAX_ASPECT_RATIOS - 1; i++)
        {
            if (context.state.ar_histogram[i].frames < context.state.ar_histogram[i + 1].frames)
            {
                hadToSwap = true;
                tempFrames = context.state.ar_histogram[i].frames;
                tempRatio  = context.state.ar_histogram[i].ar_ratio;
                context.state.ar_histogram[i] = context.state.ar_histogram[i + 1];
                context.state.ar_histogram[i+1].frames = tempFrames;
                context.state.ar_histogram[i+1].ar_ratio = tempRatio;
            }
        }
    }
    while (hadToSwap);

    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        totalFrames += context.state.ar_histogram[i].frames;
    }

    tempCount = 0;
    DetectionDebug(context, 10, "detection_histogram_sorted", std::format("{}", counter));
    i = 0;
    while (i < MAX_ASPECT_RATIOS && context.state.ar_histogram[i].frames > 0)
    {
        tempCount += context.state.ar_histogram[i].frames;
        DetectionDebug(context, 10, "detection_aspect_histogram_row",
            std::format("{:5.2f}", context.state.ar_histogram[i].ar_ratio),
            std::format("{:6}", context.state.ar_histogram[i].frames),
            std::format("{:3.1f}", static_cast<double>(tempCount) / totalFrames * 100));
        i++;
    }

}


void FillACHistogram(RecordingContext& context, bool refill)
{
    int		i;
    bool	hadToSwap;
    long	tempFrames;
    int	    tempAC;
    long	totalFrames = 0;
    long	tempCount;
    long	counter;
    int		hi;

    if (refill)
    {

        for (i = 0; i < MAX_AUDIO_CHANNELS; i++)
        {
            context.state.ac_histogram[i].frames = 0;
            context.state.ac_histogram[i].audio_channels = 0.0;
        }

        for (i = 0; i < context.state.ac_block_count; i++)
        {
            hi = context.state.ac_block[i].audio_channels;
            if (hi >= 0 && hi < MAX_AUDIO_CHANNELS)
            {
                context.state.ac_histogram[hi].frames += context.state.ac_block[i].end - context.state.ac_block[i].start + 1;
                context.state.ac_histogram[hi].audio_channels = context.state.ac_block[i].audio_channels;
            }
        }
    }

    counter = 0;
    do
    {
        hadToSwap = false;
        counter++;
        for (i = 0; i < MAX_AUDIO_CHANNELS - 1; i++)
        {
            if (context.state.ac_histogram[i].frames < context.state.ac_histogram[i + 1].frames)
            {
                hadToSwap = true;
                tempFrames = context.state.ac_histogram[i].frames;
                tempAC  = context.state.ac_histogram[i].audio_channels;
                context.state.ac_histogram[i] = context.state.ac_histogram[i + 1];
                context.state.ac_histogram[i+1].frames = tempFrames;
                context.state.ac_histogram[i+1].audio_channels = tempAC;
            }
        }
    }
    while (hadToSwap);

    for (i = 0; i < MAX_AUDIO_CHANNELS; i++)
    {
        totalFrames += context.state.ac_histogram[i].frames;
    }

    tempCount = 0;
    DetectionDebug(context, 10, "detection_histogram_sorted", std::format("{}", counter));
    i = 0;
    while (i < MAX_AUDIO_CHANNELS && context.state.ac_histogram[i].frames > 0)
    {
        tempCount += context.state.ac_histogram[i].frames;
        DetectionDebug(context, 10, "detection_audio_histogram_row",
            std::format("{:3}", context.state.ac_histogram[i].audio_channels),
            std::format("{:6}", context.state.ac_histogram[i].frames),
            std::format("{:3.1f}", static_cast<double>(tempCount) / totalFrames * 100));
        i++;
    }

}




void InsertBlackFrame(RecordingContext& context, int f, int b, int u, int v, int c)
{
    int i;

    //		if ((black_count==0 || black[black_count-1].frame < logo_block[logo_block_count-1].end )) {

    i = 0;
    while (i < context.state.black_count && context.state.black[i].frame != f)
        i++;

    if (i < context.state.black_count && context.state.black[i].frame == f)
    {
        context.state.black[i].cause |= c;
    }
    else
    {
        InitializeBlackArray(context, context.state.black_count);


        //	InitializeBlackArray(black_count);
        context.state.black_count++;
        i = context.state.black_count-2;
        while (i >= 0 && context.state.black[i].frame > f)
        {
            context.state.black[i+1] = context.state.black[i];
            i--;
        }
        i++;

        context.state.black[i].frame = f;
        context.state.black[i].brightness = b;
        context.state.black[i].uniform = u;
        context.state.black[i].volume = v;
        context.state.black[i].cause = c;
    }
}



bool BuildMasterCommList(RecordingContext& context)
{
    int		i, j, t, c;
    int		a = 0,k,count = 0;
    int		cp=0,cpf, maxsc,rsc;
    int 	silence_count = 0;
    int		silence_start = 0;
    int		summed_volume1 = 0;
    int		summed_volume2 = 0;
    int 	schange_found = false;
    int		schange_frame;
    int		low_volume_count;
    int		very_low_volume_count;
    int		schange_max;
    int		mv=0,ms=0;
    int		volume_delta;
    int		p_vol, n_vol;
    int		plataus = 0;
    std::array<int,256> platauHistogram{};

    double	length;
    double	new_ar_ratio;
    FILE*	logo_file = NULL;
    bool	foundCommercials = false;
    time_t	ltime;

    if (context.state.frame_count == 0)
    {
        Debug(context, 1, "%s", context.translator.text("detection_no_video"));
        return(false);
    }
    DetectionDebug(context, 7, "detection_scan_finished");


//    if (fabs(avg_fps - fps)> 0.01)
//        Debug(1,"WARNING: Actual framerate (%6.3f) different from specified framerate (%6.3f)\n", avg_fps, fps);


    length = F2L(context.state.frame_count-1, 1);
    if (fabs( length - (context.state.frame_count -1)/context.settings.fps) > 0.5) {
        if (fabs(context.state.avg_fps - context.settings.fps)> 1)
            Debug(context, 1, "%s", context.translator.format("detection_framerate_warning",
                std::format("{:6.3f}", context.state.avg_fps), std::format("{:6.3f}", context.settings.fps)).c_str());
        Debug(context, 1, "%s", context.translator.text("detection_timeline_warning"));
    }

    context.state.frame[context.state.frame_count].pts = context.state.frame[context.state.frame_count-1].pts + 1.0 / context.settings.fps;


    for (i = 1; i < 255; i++)
    {
        if (context.state.volumeHistogram[i] > 10)
        {
            context.state.min_volume = (i-1)*context.state.volumeScale;
            break;
        }
    }

    for (k = 1; k < 255; k++)
    {
        if (context.state.uniformHistogram[k] > 10)
        {
            context.state.min_uniform = (k-1)*UNIFORMSCALE;
            break;
        }
    }

    for (i = 0; i < 255; i++)
    {
        if (context.state.brightHistogram[i] > 1)
        {
            context.state.min_brightness_found = i;
            break;
        }
    }


    context.state.logoPercentage = (double) context.state.frames_with_logo / (double) context.state.framenum_real;

//	if (max_volume == 0)
    {

#define VOLUME_DELTA	10
#define VOLUME_MAXIMUM	300
#define VOLUME_PLATAU_SIZE		6

        volume_delta = VOLUME_DELTA;

try_again:
        if (context.state.framearray)  			// Find silence volume level
        {

            platauHistogram.fill(0);
            plataus = 0;
            j = 1;
            for (i = VOLUME_PLATAU_SIZE; i < context.state.frame_count-VOLUME_PLATAU_SIZE;)
            {
                if (context.state.frame[i].volume > VOLUME_MAXIMUM || context.state.frame[i].volume < 0)
                {
                    i++;
                    continue;
                }
                while (i+1 < context.state.frame_count-VOLUME_PLATAU_SIZE && context.state.frame[i+1].volume < context.state.frame[i].volume)
                    i++;
                k = 1;
                while (i-k - VOLUME_PLATAU_SIZE > 1 &&
                        (abs(context.state.frame[i-k].volume - context.state.frame[i].volume) < volume_delta
                         //|| frame[i-k].volume < 50
                        ))
                {
                    k++;
                }
                if (context.state.frame[i-k].volume < context.state.frame[i].volume)
                {
                    i++;
                    continue;
                }
                a = 1;
                while (i+a +VOLUME_PLATAU_SIZE < context.state.frame_count &&
                        (abs(context.state.frame[i+a].volume - context.state.frame[i].volume) < volume_delta
                         //|| frame[i+a].volume < 50
                        ))
                {
                    a++;
                }
                if (context.state.frame[i+a].volume < context.state.frame[i].volume)
                {
                    i = i+a;
                    continue;
                }
// i=8 k=1 a=11
                if (a+k > VOLUME_PLATAU_SIZE && i-k-VOLUME_PLATAU_SIZE > 0 && i+a+VOLUME_PLATAU_SIZE < context.state.frame_count)
                {
                    p_vol = (context.state.frame[i-k-4].volume +
                             context.state.frame[i-k-VOLUME_PLATAU_SIZE+3].volume +
                             context.state.frame[i-k-VOLUME_PLATAU_SIZE+2].volume +
                             context.state.frame[i-k-VOLUME_PLATAU_SIZE+1].volume +
                             context.state.frame[i-k-VOLUME_PLATAU_SIZE].volume) / 5;
                    n_vol = (context.state.frame[i+a+4].volume +
                             context.state.frame[i+a+VOLUME_PLATAU_SIZE-3].volume +
                             context.state.frame[i+a+VOLUME_PLATAU_SIZE-2].volume +
                             context.state.frame[i+a+VOLUME_PLATAU_SIZE-1].volume +
                             context.state.frame[i+a+VOLUME_PLATAU_SIZE].volume) / 5;
                    if ( p_vol > context.state.frame[i].volume + 220 || n_vol > context.state.frame[i].volume + 220 )
                        //if ( abs(frame[i-k-2].volume - frame[i].volume) > VOLUME_DELTA*2 ||
                        //	abs(frame[i+a+2].volume - frame[i].volume) > VOLUME_DELTA*2)
                    {
                        DetectionDebug(context, 8, "detection_volume_plateau", std::format("{}", i),
                            std::format("{}", k + a), std::format("{}", context.state.frame[i].volume),
                            std::format("{}", static_cast<int>(F2L(i, j))));
                        j = i;
//						for (j = i-k; j < i + a; j++)
//							frame[j].isblack |= C_v;

                        if (const auto bucket=comskip::detection::volume_histogram_bucket(
                                context.state.frame[i].volume,platauHistogram.size())) {
                            plataus++;
                            platauHistogram[*bucket]++;
                        }
                    }
                }
                i += a;
            }
            a = 0;
            DetectionDebug(context, 9, "detection_volume_histogram_heading");
            for (i = 0; i < 255; i++)
            {
                a += platauHistogram[i];
                if (platauHistogram[i] > 0)
                    Debug(context, 9, "%3d : %d\n", i*10, platauHistogram[i]);

            }
            a = a * 6 / 10;
            j = 0;
            i = 0;
            while (j < a)
            {
                j += platauHistogram[i++];
            }
            ms = i*10;
            DetectionDebug(context, 7, "detection_silence_level", std::format("{}", ms));
            if (ms > 0 && ms < 10)
                ms = 10;
            if (ms < 50)
                mv = 1.5 * ms;
            else if (ms < 100)
                mv = 2 * ms;
            else if (ms < 200)
                mv = 2 * ms;
            else
                mv = 2 * ms;
        }
        if (mv == 0 || plataus < 5)
        {
            volume_delta *= 2;
            if (volume_delta < VOLUME_MAXIMUM)
                goto try_again;
        }
    }
    if (context.settings.max_volume == 0)
    {
        context.settings.max_volume = mv;
        context.settings.max_silence = ms;
    }
    /*
        if (max_silence < min_volume + 30)
            max_silence = min_volume + 30;

        if (max_volume < 100) {
            if ( max_volume < min_volume + 30)
                max_volume = min_volume + 30;
        }
        else
        if (max_volume < min_volume + 100)
            max_volume = min_volume + 100;
    */
    if (context.settings.max_volume == 0)
    {

        if (context.state.framearray)  			// Find silence volume level
        {

#define START_VOLUME	500
            count = 21;
scanagain:
            a = START_VOLUME;
            k = 0;
            if (context.state.frame[i].volume > 0)
                j = context.state.frame[i].volume;
            else
                j = 0;
            for (i = 1; i < context.state.frame_count; i++ )
            {
                if (context.state.frame[i].volume > 0 && context.state.frame[i].volume < a)
                {
                    if (context.state.frame[i].volume < j)
                        j = context.state.frame[i].volume;
                    k++;
                    if (k > count && a > context.state.frame[i-count].volume + 20 &&
                            j > a - 250)
                    {
                        i = i - count;
                        a = context.state.frame[i].volume;
                        j = context.state.frame[i].volume;
                        k = 0;
                    }
                }
                else
                {
                    k = 0;
                    if (context.state.frame[i].volume > 0)
                        j = context.state.frame[i].volume;
                }
            }
        }
        if (a > START_VOLUME-100 && count > 7)
        {
            count = count - 7;
            goto scanagain;
        }
        context.settings.max_silence = a+10;
        context.settings.max_volume = a+150;
    }

    if (context.settings.max_volume == 0)
    {

        for (k = 2; k < 255; k++)
        {
            if (context.state.volumeHistogram[k] > 10)
            {
                context.settings.max_volume = k*context.state.volumeScale + 200;
                context.settings.max_silence = k*context.state.volumeScale + 20;
                break;
            }

        }
        /*

                max_volume = 1000;
                for (k = black_count - 1; k >= 0; k--) {
                    if (black[k].volume >= 0 && black[k].volume <  max_volume)
                        max_volume = black[k].volume;
                }
                max_volume *= 4;
         */
        DetectionDebug(context, 1, "detection_setting_max_volume",
            std::format("{}", context.settings.max_volume));
    }

    if (context.settings.commDetectMethod & LOGO)
    {
        // close out last logo cblock if one is open
        ProcessLogoTest(context, context.state.frame_count, false, true);
        /*
                if (loadingCSV) {
                    prev_logo_threshold = logo_threshold-1.0;
                    FindLogoThreshold();
                    if (fabs(logo_threshold - prev_logo_threshold) > 0.4) {
                        Debug(2,"Changed logo_threshold to %.2f, recalculating logo timeline\n", logo_threshold);
                        InitProcessLogoTest();
                        for (i = 1; i < frame_count; i++) {
                            curLogoTest = (frame[i].currentGoodEdge > logo_threshold);
                            lastLogoTest = ProcessLogoTest(i, curLogoTest);
                            frame[i].logo_present = lastLogoTest;
                            if (lastLogoTest) frames_with_logo++;
                        }
                        logoPercentage = (double) frames_with_logo / (double) framenum_real;
                    }
                    else
                        logo_threshold = prev_logo_threshold;

                }
        */

        if (context.state.logo_quality == 0.0)
            FindLogoThreshold(context);

        // Clean up logo blocks
        /*
                for (i = logo_block_count-2; i >= 0; i--) {
                    if (F2L(logo_block[i+1].start, logo_block[i].end) < min_commercial_size + (2*shrink_logo)) {
                        Debug(1, "Logo cblock %d and %d combined because gap (%i s) too short with previous\n", i, i+1, (int)F2L(logo_block[i+1].start, logo_block[i].end ));
                        logo_block[i+1].start = logo_block[i].start;
                        for (t = i; t+1 < logo_block_count; t++) {
                            logo_block[t] = logo_block[t+1];
                        }
                        logo_block_count--;
                    }
                }
        */
        for (i = context.state.logo_block_count-1; i >= 0; i--)
        {
            if (F2L(context.state.logo_block[i].end, context.state.logo_block[i].start) < context.settings.min_commercial_size - 2*context.settings.shrink_logo)
            {
                DetectionDebug(context, 1, "detection_logo_block_too_short",
                    std::format("{}", i), std::format("{}", static_cast<int>(F2L(
                        context.state.logo_block[i].end, context.state.logo_block[i].start))));
                for (t = i; t+1 < context.state.logo_block_count; t++)
                {
                    context.state.logo_block[t] = context.state.logo_block[t+1];
                }
                context.state.logo_block_count--;
            }
        }
        if (context.state.logoPercentage > context.settings.logo_fraction && context.settings.after_logo > 0 && context.state.framearray && context.state.logo_block_count > 0)
        {
            for (i = 0; i < context.state.logo_block_count; i++)
            {
                if (i < context.state.logo_block_count-1 && F2L(context.state.logo_block[i+1].start, context.state.logo_block[i].end)< context.settings.max_commercialbreak/4)
                    continue;			// Don't do anything if too close
                if (i == context.state.logo_block_count-1 && F2L(context.state.frame_count, context.state.logo_block[i].end) < context.settings.max_commercialbreak/4)
                    continue;			// Don't do anything if too close
                if (context.settings.after_logo==999)
                {
                    j = context.state.logo_block[i].end;
                    InsertBlackFrame(context, j,context.state.frame[j].brightness,context.state.frame[j].uniform,0, C_l);
                    DetectionDebug(context, 3, "detection_logo_cut_disappears",
                        std::format("{:6}", j), std::format("{:.3f}", get_frame_pts(context, j)));
                    continue;
                }

                j = context.state.logo_block[i].end + (int)(context.settings.after_logo * context.settings.fps);
                if ( j >= context.state.frame_count)
                    j = context.state.frame_count-1;
                t = j + (int)(30 * context.settings.fps);
                if ( t >= context.state.frame_count)
                    t = context.state.frame_count-1;
                maxsc = 255;
                cp = 0;
                cpf = 0;
                while (j < t)
                {
                    rsc = 255;
                    while (context.state.frame[j].volume >= context.settings.max_volume && j < t)
                    {
//						if (rsc > frame[j].schange_percent)
//							rsc = frame[j].schange_percent;
                        j++;
                    }
                    if (j == t )
                        break;
                    c = 10;
                    j = j - 10;
                    if (j < 1)
                    {
                        j = j - 1;
                        c = c +j;
                        j = 1;
                    }
                    while (c-- && j < t)
                    {
                        if (rsc > context.state.frame[j].schange_percent)
                        {
                            rsc = context.state.frame[j].schange_percent;
                            cpf = j;
                        }
                        j++;
                    }
                    if (j == t )
                        break;
                    while (context.state.frame[j].volume < context.settings.max_volume && j < t)
                    {
                        if (rsc > context.state.frame[j].schange_percent)
                        {
                            rsc = context.state.frame[j].schange_percent;
                            cpf = j;
                        }
                        j++;
                    }
                    if (j == t )
                        break;
                    c = 10;
                    while (c-- && j < t)
                    {
                        if (rsc > context.state.frame[j].schange_percent)
                        {
                            rsc = context.state.frame[j].schange_percent;
                            cpf = j;
                        }
                        j++;
                    }
                    if (j == t )
                        break;
                    if (maxsc > rsc)
                    {
                        maxsc = rsc;
                        cp = cpf;
                    }
                    j = t; // Only search once
//					cp = j;
//					j=t;
                }
                if (cp != 0)
                {
                    InsertBlackFrame(context, cp,context.state.frame[cp].brightness,context.state.frame[cp].uniform,context.state.frame[cp].volume, C_l);
                    DetectionDebug(context, 3, "detection_logo_cut_after_disappears",
                        std::format("{:6}", cp), std::format("{:.3f}", get_frame_pts(context, cp)),
                        std::format("{}", static_cast<int>(F2L(cp, context.state.logo_block[i].end))),
                        std::format("{}", maxsc));
                }
            }
        }

        if (context.state.logoPercentage > context.settings.logo_fraction && context.settings.before_logo > 0 && context.state.framearray && context.state.logo_block_count > 0)
        {
            for (i = 0; i < context.state.logo_block_count; i++)
            {
                if (i > 0 && F2L(context.state.logo_block[i].start, context.state.logo_block[i-1].end) < context.settings.max_commercialbreak/4)
                    continue;
                if (i == 0 && F2T(context.state.logo_block[i].start) < context.settings.max_commercialbreak/4)
                    continue;
                if (context.settings.before_logo==999)
                {
                    j = context.state.logo_block[i].start;
                    InsertBlackFrame(context, j,context.state.frame[j].brightness,context.state.frame[j].uniform,0, C_l);
                    DetectionDebug(context, 3, "detection_logo_cut_appears",
                        std::format("{:6}", j), std::format("{:.3f}", get_frame_pts(context, j)));

                    continue;
                }

                j = context.state.logo_block[i].start - (int)(context.settings.before_logo * context.settings.fps);
                if ( j < 1)
                    j = 1;
                t = j - (int)(30 * context.settings.fps);
                if ( t < 1)
                    t = 1;
                maxsc = 255;
                cp = 0;
                cpf = 0;
                while (j > t)
                {
                    rsc = 255;
                    while (context.state.frame[j].volume >= context.settings.max_volume && j > t) // Search low volume
                    {
//						if (rsc > frame[j].schange_percent)
//							rsc = frame[j].schange_percent;
                        j--;
                    }
                    if (j == t )
                        break;
                    c = 10;
                    j = j + 10;
                    if (j >= context.state.frame_count)
                    {
                        j = j - context.state.frame_count;
                        c = c - j;
                        j = context.state.frame_count - 1;
                    }
                    while (c-- && j > t) // Largest scene change 10 frames before low volume
                    {
                        if (rsc > context.state.frame[j].schange_percent)
                        {
                            rsc = context.state.frame[j].schange_percent;
                            cpf = j;
                        }
                        j--;
                    }
                    if (j == t )
                        break;

                    while (context.state.frame[j].volume < context.settings.max_volume && j > t) // largest scene change in low volume
                    {
                        if (rsc > context.state.frame[j].schange_percent)
                        {
                            rsc = context.state.frame[j].schange_percent;
                            cpf = j;
                        }
                        j--;
                    }
                    if (j == t )
                        break;
                    c = 10;
                    while (c-- && j > t) //Largest scene change after low volume
                    {
                        if (rsc > context.state.frame[j].schange_percent)
                        {
                            rsc = context.state.frame[j].schange_percent;
                            cpf = j;
                        }
                        j--;
                    }
                    if (j == t )
                        break;
                    if (maxsc > rsc)
                    {
                        maxsc = rsc;
                        cp = cpf;
                    }
                    j = t; // Only search once
//					cp = j;
//					j=t;
                }
                if (cp != 0)
                {
                    InsertBlackFrame(context, cp,context.state.frame[cp].brightness,context.state.frame[cp].uniform,context.state.frame[cp].volume, C_l);
                    DetectionDebug(context, 3, "detection_logo_cut_before_appears",
                        std::format("{:6}", cp), std::format("{:.3f}", get_frame_pts(context, cp)),
                        std::format("{}", static_cast<int>(F2L(context.state.logo_block[i].start, cp))),
                        std::format("{}", maxsc));
                }
            }
        }
//		if (logoPercentage > .15 && logoPercentage < .32 ) {
//			reverseLogoLogic = true;
//			logoPercentage = 1 - logoPercentage;
//		}
        if (context.state.logoPercentage < context.settings.logo_fraction - 0.05 || context.state.logoPercentage > context.settings.logo_percentile)
        {
            Debug(context, 1, "%s", context.translator.format("detection_logo_disabled",
                std::format("{:.2f}", context.state.logoPercentage)).c_str());
            context.settings.commDetectMethod -= LOGO;
        }
    }

    if (context.settings.remove_silent_segments) {
        i = 1;
        j = 1;
        for (i=1; i < context.state.frame_count; i++)
        {
            if (context.state.frame[i].volume < 5) {
                j = i+1;
                while (j < context.state.frame_count && context.state.frame[j].volume < 10 ) j++;
                if ((context.state.frame[j-1].pts - context.state.frame[i].pts) > context.settings.remove_silent_segments) {
                    DetectionDebug(context, 4, "detection_long_silent_segment",
                        std::format("{}", i), std::format("{}", j - 1));
                    InsertBlackFrame(context, i,context.state.frame[i].brightness,context.state.frame[i].uniform,context.state.frame[i].volume, C_v);
                    InsertBlackFrame(context, j-1,context.state.frame[j-1].brightness,context.state.frame[j-1].uniform,context.state.frame[j].volume, C_v);
                }
                i = j + 1;
            }
        }
    }

    if (context.settings.commDetectMethod & SILENCE)
    {
        silence_count = 0;
        schange_found = false;
        schange_frame = 0;
        schange_max = 100;
        low_volume_count = 0;
        very_low_volume_count = 0;
        for (i=1; i <context.state.frame_count; i++)
        {
            if (context.state.frame[i].volume < 6)
            {
                InsertBlackFrame(context, i,context.state.frame[i].brightness,context.state.frame[i].uniform,context.state.frame[i].volume, C_v);
            } else
            if (context.settings.min_silence > 0)
            {
                if (0 <= context.state.frame[i].volume && context.state.frame[i].volume < context.settings.max_silence)
                {
                    if (silence_start == 0)
                        silence_start = i;
                    silence_count++;
                    if (context.state.frame[i].schange_percent < context.state.schange_threshold)
                    {
                        schange_found = true;
                        if (schange_max > context.state.frame[i].schange_percent)
                        {
                            schange_frame = i;
                            schange_max = context.state.frame[i].schange_percent;
                        }
                    }
                    if (context.state.frame[i].uniform < context.settings.non_uniformity)
                    {
                        schange_found = true;
                        schange_frame = i;
                    }
                    if (context.state.frame[i].volume < context.settings.max_silence)
                    {
                        low_volume_count++;
                    }
                    if (context.state.frame[i].volume < 9)
                    {
                        very_low_volume_count++;
                    }
                }
                else
                {
                    if (silence_count > context.settings.min_silence /* * (int)fps */ && silence_count < 5 * context.settings.fps)
                    {

                        if ( very_low_volume_count > (int)(silence_count * 0.7) ||  schange_found || context.state.frame[i].schange_percent < context.state.schange_threshold)
                        {
#define SILENCE_CHECK	((int)(2.5 * context.settings.fps))
                            summed_volume1 = 0;
                            for (j = max(silence_start - SILENCE_CHECK,1); j < silence_start; j++)
                            {
//							if (summed_volume1 < frame[j].volume)
                                summed_volume1 += context.state.frame[j].volume;
                            }
                            summed_volume1 /= min(SILENCE_CHECK, silence_start+1) ;
                            summed_volume2 = 0;
                            for (j = i; j < min(i+SILENCE_CHECK, context.state.frame_count); j++)
                            {
//							if (summed_volume2 < frame[j].volume)
                                summed_volume2 += context.state.frame[j].volume;
                            }
                            summed_volume2 /= min(SILENCE_CHECK, context.state.frame_count - i + 1);
                            if ((summed_volume1 > 0.9*context.settings.max_volume &&  summed_volume2 > 0.9*context.settings.max_volume && low_volume_count > context.settings.min_silence ) ||
                                    (summed_volume1 > 2*context.settings.max_volume &&  summed_volume2 > 2*context.settings.max_volume) ||
                                    (summed_volume1 > 4*context.settings.max_volume ||  summed_volume2 > 4*context.settings.max_volume) ||
                                    very_low_volume_count  > context.settings.min_silence
                               )
                            {
                                if (schange_frame == 0)
                                    schange_frame = i;

#if 1
                                for (j=silence_start; j < i; j++)
                                {
                                    context.state.frame[j].isblack |= C_v;
                                    InsertBlackFrame(context, j,context.state.frame[j].brightness,context.state.frame[j].uniform,context.state.frame[j].volume, C_v);
                                }
#else
                                context.state.frame[schange_frame].isblack |= C_v;
                                InsertBlackFrame(schange_frame,context.state.frame[schange_frame].brightness,context.state.frame[schange_frame].uniform,context.state.frame[schange_frame].volume, C_v);
#endif
                                //for (j = silence_start /*i - min_silence /* * (int)fps */; j <= i; j++) {
                                //	frame[j].isblack |= C_v;
                                //	InsertBlackFrame(j,frame[j].brightness,frame[j].uniform,frame[j].volume, C_v);
                                //}
                            }
                        }
                    }
                    silence_start = 0;
                    silence_count = 0;
                    schange_found = false;
                    schange_frame = 0;
                    schange_max = 100;
                    low_volume_count = 0;
                    very_low_volume_count = 0;
                }
            }
        }
    }


    context.state.after_start = context.settings.added_recording * context.settings.fps * 60;
    context.state.before_end  = context.state.frame_count - context.settings.added_recording * context.settings.fps * 60;

    context.state.frame[context.state.frame_count].dimCount = 0;
    context.state.frame[context.state.frame_count].hasBright = 0;
    InsertBlackFrame(context, context.state.frame_count,0,0,0, C_b);

    if (context.settings.cut_on_ac_change)
    {
        if (context.state.ac_block[context.state.ac_block_count].start > 0)
        {
            DetectionDebug(context, 5, "detection_last_ar_block_open");
            context.state.ac_block[context.state.ac_block_count].end = context.state.frame_count;
            context.state.ac_block_count++;
        }

        FillACHistogram(context, true);
        context.state.dominant_ac = context.state.ac_histogram[0].audio_channels;

        // Print out ar cblock list
        DetectionDebug(context, 4, "detection_ac_blocks_heading");
        for (i = 0; i < context.state.ac_block_count; i++)
        {
            DetectionDebug(context, 4, "detection_ac_block_row", std::format("{}", i),
                std::format("{:6}", context.state.ac_block[i].start),
                std::format("{:6}", context.state.ac_block[i].end),
                std::format("{:2}", context.state.ac_block[i].audio_channels),
                dblSecondsToStrMinutes(context, F2L(context.state.ac_block[i].end,
                    context.state.ac_block[i].start)));
        }
    }

    // close out the last ar cblock
    if (context.settings.commDetectMethod & AR)
    {
        const auto debug_ar_block = [&](int level, int index) {
            const auto& block = context.state.ar_block[index];
            DetectionDebug(context, level, "detection_ar_block_row", std::format("{}", index),
                std::format("{:6}", block.start), std::format("{:6}", block.end),
                std::format("{:.2f}", block.ar_ratio),
                dblSecondsToStrMinutes(context, F2L(block.end, block.start)),
                std::format("{:4}", block.width), std::format("{:4}", block.height),
                std::format("{:3}", block.minX), std::format("{:3}", block.minY),
                std::format("{:3}", block.maxX), std::format("{:3}", block.maxY));
        };
        if (context.state.ar_block[context.state.ar_block_count].start > 0)
        {
            DetectionDebug(context, 5, "detection_last_ar_block_open");
            context.state.ar_block[context.state.ar_block_count].end = context.state.frame_count;
            context.state.ar_block_count++;
        }


        // Print out ar cblock list
        DetectionDebug(context, 9, "detection_ar_blocks_before_heading");
        for (i = 0; i < context.state.ar_block_count; i++)
        {
            debug_ar_block(9, i);
        }

        // Calculate histogram with noisy aspect ratios
        FillARHistogram(context, false);

        // Update histogram to remove replaced ratios
        for (i = 0 ; i < MAX_ASPECT_RATIOS; i++)
        {
            for (j = i+1; j < MAX_ASPECT_RATIOS; j++)
            {
                if (context.state.ar_histogram[j].ar_ratio < context.state.ar_histogram[i].ar_ratio+context.settings.ar_delta &&
                        context.state.ar_histogram[j].ar_ratio > context.state.ar_histogram[i].ar_ratio-context.settings.ar_delta )
                    context.state.ar_histogram[j].ar_ratio = context.state.ar_histogram[i].ar_ratio;
            }

        }

        // Normalize aspect ratios
        for (i = 0; i < context.state.ar_block_count; i++)
        {
            new_ar_ratio = FindARFromHistogram (context, context.state.ar_block[i].ar_ratio);
            context.state.ar_block[i].ar_ratio = new_ar_ratio;
        }
        // Calculate histogram with normalized aspect ratios
        FillARHistogram(context, true);
        context.state.dominant_ar = context.state.ar_histogram[0].ar_ratio;


again:
        // Clean up ar cblock list

        for (i = context.state.ar_block_count - 1; i > 0; i--)
        {
            length = context.state.ar_block[i].end - context.state.ar_block[i].start;

            if (context.settings.cut_on_ar_change > 2 && length < context.settings.cut_on_ar_change*(int)context.settings.fps && context.state.ar_block[i].ar_ratio != AR_UNDEF )
            {
                DetectionDebug(context, 6, "detection_ar_block_undefine", std::format("{}", i));
                context.state.ar_block[i].ar_ratio = AR_UNDEF;
                goto again;
            }

            /*
                        if (ar_block[i].ar_ratio == AR_UNDEF && length < 5*(int)fps) {
                            ar_block[i - 1].end = ar_block[i].end;
                            ar_block_count--;
                            Debug(
                                6,
                                "Deleting AR cblock %i because it is too short\n",
                                i,
                                dblSecondsToStrMinutes(length / fps)
                            );
                            for (j = i; j < ar_block_count; j++) {
                                ar_block[j].start = ar_block[j + 1].start;
                                ar_block[j].end = ar_block[j + 1].end;
                                ar_block[j].ar_ratio = ar_block[j + 1].ar_ratio;
                            }
                            goto again;
                        }
            */
#if 1
            if (context.settings.commDetectMethod & LOGO && 	context.state.ar_block[i - 1].ar_ratio != AR_UNDEF &&
                    context.state.ar_block[i].ar_ratio > context.state.ar_block[i - 1].ar_ratio &&
                    CheckFrameForLogo(context, context.state.ar_block[i-1].end) &&
                    CheckFrameForLogo(context, context.state.ar_block[i].start) )
            {
                if (context.state.ar_block[i].end - context.state.ar_block[i].start > context.state.ar_block[i-1].end - context.state.ar_block[i-1].start)
                {
                    j = context.state.ar_block[i-1].start;
                    context.state.ar_block[i-1] = context.state.ar_block[i];
                    context.state.ar_block[i-1].start = j;
                }
                else
                    context.state.ar_block[i - 1].end = context.state.ar_block[i].end;
                context.state.ar_block_count--;
                DetectionDebug(context, 6, "detection_ar_join_logo",
                    std::format("{}", i - 1), std::format("{}", i));
                for (j = i; j < context.state.ar_block_count; j++)
                {
                    context.state.ar_block[j] = context.state.ar_block[j + 1];
                }
                goto again;
            }
//
#endif
            if ( i == 1 && context.state.ar_block[i-1].ar_ratio == AR_UNDEF)
            {
                j = context.state.ar_block[i - 1].start;
                context.state.ar_block[i - 1] = context.state.ar_block[i];
                context.state.ar_block[i - 1].start = j;
                context.state.ar_block_count--;
                DetectionDebug(context, 6, "detection_ar_join_first_undefined",
                    std::format("{}", i - 1), std::format("{}", i));
                for (j = i; j < context.state.ar_block_count; j++)
                {
                    context.state.ar_block[j] = context.state.ar_block[j + 1];
                }
                goto again;

            }
            if (( context.state.ar_block[i].ar_ratio - context.state.ar_block[i - 1].ar_ratio < context.settings.ar_delta &&
                    context.state.ar_block[i].ar_ratio - context.state.ar_block[i - 1].ar_ratio > -context.settings.ar_delta ))
            {
                context.state.ar_block[i - 1].end = context.state.ar_block[i].end;
                context.state.ar_block_count--;
                DetectionDebug(context, 6, "detection_ar_join_same_ratio",
                    std::format("{}", i - 1), std::format("{}", i),
                    std::format("{:.2f}", context.state.ar_block[i].ar_ratio));
                for (j = i; j < context.state.ar_block_count; j++)
                {
                    context.state.ar_block[j] = context.state.ar_block[j + 1];
                }
                goto again;

            }
            if (  context.state.ar_block[i-1].ar_ratio == AR_UNDEF && i > 1 &&
                    context.state.ar_block[i].ar_ratio - context.state.ar_block[i - 2].ar_ratio < context.settings.ar_delta &&
                    context.state.ar_block[i].ar_ratio - context.state.ar_block[i - 2].ar_ratio > -context.settings.ar_delta )
            {
                context.state.ar_block[i - 2].end = context.state.ar_block[i].end;
                context.state.ar_block_count -= 2;
                DetectionDebug(context, 6, "detection_ar_join_dummy",
                    std::format("{}", i - 2), std::format("{}", i));
                for (j = i-1; j < context.state.ar_block_count; j++)
                {
                    context.state.ar_block[j] = context.state.ar_block[j + 2];
                }
                goto again;

            }
        }

        // Print out ar cblock list
        DetectionDebug(context, 4, "detection_ar_blocks_heading");
        for (i = 0; i < context.state.ar_block_count; i++)
        {
            debug_ar_block(4, i);
        }
    }

    // close out the last cc cblock
    if (context.state.processCC)
    {
        context.state.cc_block[context.state.cc_block_count].end_frame = context.state.frame_count;
        context.state.cc_block_count++;
        context.state.cc_text[context.state.cc_text_count].end_frame = context.state.frame_count;
        context.state.cc_text_count++;
        for (i = context.state.cc_text_count - 1; i > 0; i--)
        {
            if (context.state.cc_text[i].text_len == 0)
            {
                for (j = i; j < context.state.cc_text_count; j++)
                {
                    context.state.cc_text[j].start_frame = context.state.cc_text[j + 1].start_frame;
                    context.state.cc_text[j].end_frame = context.state.cc_text[j + 1].end_frame;
                    context.state.cc_text[j].text_len = context.state.cc_text[j + 1].text_len;
                    strncpy((char*)context.state.cc_text[j].text, (char*)context.state.cc_text[j + 1].text, sizeof(context.state.cc_text[j].text));
                }

                context.state.cc_text_count--;
            }
        }

        DetectionDebug(context, 2, "detection_caption_transcript_heading");

        for (i = 0; i < context.state.cc_text_count; i++)
        {
            Debug(context,
                2,
                "%i) S:%6i E:%6i L:%4i %s\n",
                i,
                context.state.cc_text[i].start_frame,
                context.state.cc_text[i].end_frame,
                context.state.cc_text[i].text_len,
                context.state.cc_text[i].text
            );
        }
    }

    if (context.settings.output_framearray) OutputFrameArray(context, false);
    if (context.settings.output_framearray) OutputBlackArray(context);

    BuildBlocks(context, false);
    if (context.settings.commDetectMethod & LOGO)
    {
        PrintLogoFrameGroups(context);
    }
    WeighBlocks(context);

    foundCommercials = OutputBlocks(context);

    if (context.settings.verbose)
    {
        DetectionDebug(context, 1, "detection_frames_processed",
            std::format("{}", context.state.framesprocessed));
        time(&ltime);
        const auto* timestamp=ctime(&ltime);
        comskip::output::write_run_footer(context.state.logfilename,timestamp ? timestamp : "");
    }


    if (context.settings.ccCheck && context.state.processCC)
    {
        const bool has_captions = context.state.most_cc_type == PAINTON ||
            context.state.most_cc_type == ROLLUP || context.state.most_cc_type == POPON;
        const auto marker_name = context.state.workbasename + (has_captions ? ".ccyes" : ".ccno");
        const auto marker = comskip::platform::own_file(myfopen(marker_name.c_str(), "w"));
        if (!marker)
            Debug(context, 0, "%s", context.translator.format("create_failed", strerror(errno), marker_name).c_str());
        else {
            const auto old_marker = context.state.workbasename + (has_captions ? ".ccno" : ".ccyes");
            myremove(old_marker.c_str());
        }
    }

    if (context.settings.deleteLogoFile)
    {
        logo_file = myfopen(context.state.logofilename.c_str(), "r");
        if(logo_file)
        {
            fclose(logo_file);
            myremove(context.state.logofilename.c_str());
        }
    }

//	free(frame);
    return (foundCommercials);
}
