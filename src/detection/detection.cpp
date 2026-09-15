#include "legacy_detection.h"

int DetectCommercials(int f, double pts)
{
    bool isBlack = 0;	/*Gil*/
    int i,j;
    long oldBlack_count;


    if (loadingTXT)
        return(0);
    if (loadingCSV)
        return(0);
    if (!initialized)
        InitComSkip();
//	frame_count++;
    frame_count = framenum_real = framenum+1;

//Debug(1, "Frame info f=%d, framenum=%d, framenum_real=%d, frame_count=%d\n",f, framenum, framenum_real, frame_count, max_frame_count);

    avg_fps = 1.0/ (pts / frame_count);

    if (framenum_real < 0) return 0;
    if (play_nice) sleep_for_ms(play_nice_sleep);
    if (framearray) InitializeFrameArray(framenum_real);
//	curvolume = RetreiveVolume(framenum_real);
    //curvolume = RetreiveVolume(frame_count);

    if (pts < 0.0)
        pts = 0.0;
    frame[frame_count].pts = pts;
    frame[frame_count].pict_type = pict_type;
    if (frame_count == 1)
        frame[0].pts = pts;
//    curvolume = retreive_frame_volume(get_frame_pts(frame_count-1), get_frame_pts(frame_count));
    frame[frame_count].volume = -1;
    backfill_frame_volumes();
    curvolume = frame[frame_count].volume;

//	if (frame_count != framenum_real)
//		Debug(0, "Inconsistent frame numbers\n");
    if (framearray)
    {
        frame[frame_count].volume = curvolume;
        frame[frame_count].goppos = headerpos;

        frame[frame_count].cur_segment = debug_cur_segment;
        frame[frame_count].audio_channels = audio_channels;

    }
    if (curvolume > 0)
    {
        volumeHistogram[(curvolume/volumeScale < 255 ? curvolume/volumeScale : 255)]++;
    }
    if (ticker_tape_percentage > 0)
        ticker_tape = ticker_tape_percentage * height / 100;
    if (ticker_tape > 0 )
    {
        memset(&frame_ptr[width*(height - ticker_tape)], 0, width*ticker_tape);
    }
    if (top_ticker_tape_percentage > 0)
        top_ticker_tape = top_ticker_tape_percentage * height / 100;
    if (top_ticker_tape > 0 )
    {
        memset(&frame_ptr[0], 0, width*top_ticker_tape);
    }
    if (ignore_side)
    {
        for (i = 0; i < height; i++)
        {
            for (j = 0; j < ignore_side; j++)
            {
                frame_ptr[width*i + j] = 0;
                frame_ptr[width*i + (width -1) - j] = 0;
            }
        }
    }
    if (ignore_left_side)
    {
        for (i = 0; i < height; i++)
        {
            for (j = 0; j < ignore_left_side; j++)
            {
                frame_ptr[width*i + j] = 0;
            }
        }
    }
    if (ignore_right_side)
    {
        for (i = 0; i < height; i++)
        {
            for (j = 0; j < ignore_right_side; j++)
            {
                frame_ptr[width*i + (width -1) - j] = 0;
            }
        }
    }

    oldBlack_count = black_count;	/*Gil*/
    CheckSceneHasChanged();
    isBlack = oldBlack_count != black_count;	/*Gil*/


    if ((commDetectMethod & LOGO) && ((frame_count % (int)(fps * logoFreq)) == 0))
    {
        if (!logoInfoAvailable || (!lastLogoTest && !startOverAfterLogoInfoAvail) )
        {
            if (delay_logo_search == 0 ||
                    (delay_logo_search == 1 && F2T(frame_count) > added_recording * 60) ||
                    (delay_logo_search > 1 && F2T(frame_count) > delay_logo_search))
            {
                FillLogoBuffer();
                if (logoBuffersFull)
                {
                    Debug(6, "\nLooking For Logo in frames %i to %i.\n", logoFrameNum[oldestLogoBuffer], frame_count);
                    if(!SearchForLogoEdges())
                    {
                        InitComSkip();
                        return 1;
                    }
                }
                if (logoInfoAvailable)
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
        if (logoInfoAvailable)
        {
//			EdgeCount(frame_ptr);
//			curLogoTest = logoBuffersFull;
            currentGoodEdge = CheckStationLogoEdge(frame_ptr);
            curLogoTest = (currentGoodEdge > logo_threshold);
            lastLogoTest = ProcessLogoTest(frame_count, curLogoTest, false);
            if (!lastLogoTest && !startOverAfterLogoInfoAvail && logoBuffersFull)   // Lost logo
            {
//				logoInfoAvailable = false;
//				secondLogoSearch = true;
                logoBuffersFull = false;
                InitLogoBuffers();
                newestLogoBuffer = -1;
            }
            if (startOverAfterLogoInfoAvail && !loadingCSV && !secondLogoSearch && logo_block_count > 0 &&
                    !lastLogoTest &&
                    F2L(frame_count,logo_block[logo_block_count-1].end) > ( max_commercialbreak * 1.2 ) &&
                    (double)frames_with_logo / (double)frame_count < 0.5
               )
            {
                Debug(6, "\nNo Logo in frames %i to %i, restarting Logo search.\n", logo_block[logo_block_count-1].end, frame_count);
                // First logo found but no logo found after first commercial cblock so search new logo
                logoInfoAvailable = false;
                secondLogoSearch = true;
                logoBuffersFull = false;
                InitLogoBuffers();
                newestLogoBuffer = -1;
            }
        }
    }

//	EdgeCount(frame_ptr);
//	currentGoodEdge = ((double) edge_count) / 750;

    if (logoInfoAvailable && framearray) {
      frame[frame_count].logo_present = lastLogoTest;
    } else if (framearray) {
      frame[frame_count].logo_present = 0.0;
    }
    if (lastLogoTest)
        frames_with_logo++;
    if (framearray) frame[frame_count].currentGoodEdge = currentGoodEdge;

    if (((frame_count) & subsample_video) == 0)
        OutputDebugWindow(true,frame_count,true, false);
//	key = 0;
//	while (key==0)
//		vo_wait();

    framesprocessed++;
    scr += 1;

    if (live_tv && !isBlack)
    {
        BuildCommListAsYouGo();
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


double AverageARForBlock(int start, int end)
{
    int i, maxSize;
    double Ar;
    int f,t;

    maxSize = 0;
    Ar = 0.0;
    for (i = 0; i < ar_block_count; i++)
    {
        f = max(ar_block[i].start, start);
        t = min(ar_block[i].end, end);
        if (maxSize < t-f+1)
        {
            Ar = ar_block[i].ar_ratio;
            maxSize = t-f+1;
        }
        if (ar_block[i].start > end)
            break;
    }
    return(Ar);
}

int AverageACForBlock(int start, int end)
{
    int i, maxSize;
    int Ac;
    int f,t;

    maxSize = 0;
    Ac = 0;
    for (i = 0; i < ac_block_count; i++)
    {
        f = max(ac_block[i].start, start);
        t = min(ac_block[i].end, end);
        if (maxSize < t-f+1)
        {
            Ac = ac_block[i].audio_channels;
            maxSize = t-f+1;
        }
        if (ac_block[i].start > end)
            break;
    }
    return(Ac);
}

double	FindARFromHistogram(double ar_ratio)
{
    int i;
    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        if (ar_ratio > ar_histogram[i].ar_ratio - ar_delta &&
                ar_ratio < ar_histogram[i].ar_ratio + ar_delta)
            return (ar_histogram[i].ar_ratio);
    }
    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        if (ar_ratio > ar_histogram[i].ar_ratio - 2*ar_delta &&
                ar_ratio < ar_histogram[i].ar_ratio + 2*ar_delta)
            return (ar_histogram[i].ar_ratio);
    }
    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        if (ar_ratio > ar_histogram[i].ar_ratio - 4*ar_delta &&
                ar_ratio < ar_histogram[i].ar_ratio + 4*ar_delta)
            return (ar_histogram[i].ar_ratio);
    }
    return (0.0);
}

void FillARHistogram(bool refill)
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
            ar_histogram[i].frames = 0;
            ar_histogram[i].ar_ratio = 0.0;
        }

        for (i = 0; i < ar_block_count; i++)
        {
            hi = (int)((ar_block[i].ar_ratio - 0.5)*100);
            if (hi >= 0 && hi < MAX_ASPECT_RATIOS)
            {
                ar_histogram[hi].frames += ar_block[i].end - ar_block[i].start + 1;
                ar_histogram[hi].ar_ratio = ar_block[i].ar_ratio;
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
            if (ar_histogram[i].frames < ar_histogram[i + 1].frames)
            {
                hadToSwap = true;
                tempFrames = ar_histogram[i].frames;
                tempRatio  = ar_histogram[i].ar_ratio;
                ar_histogram[i] = ar_histogram[i + 1];
                ar_histogram[i+1].frames = tempFrames;
                ar_histogram[i+1].ar_ratio = tempRatio;
            }
        }
    }
    while (hadToSwap);

    for (i = 0; i < MAX_ASPECT_RATIOS; i++)
    {
        totalFrames += ar_histogram[i].frames;
    }

    tempCount = 0;
    Debug(10, "\n\nAfter Sorting - %i\n--------------\n", counter);
    i = 0;
    while (i < MAX_ASPECT_RATIOS && ar_histogram[i].frames > 0)
    {
        tempCount += ar_histogram[i].frames;
        Debug(10, "Aspect Ratio  %5.2f found on %6i frames totalling \t%3.1f%c\n", ar_histogram[i].ar_ratio, ar_histogram[i].frames, ((double)tempCount / (double)totalFrames)*100,'%');
        i++;
    }

}


void FillACHistogram(bool refill)
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
            ac_histogram[i].frames = 0;
            ac_histogram[i].audio_channels = 0.0;
        }

        for (i = 0; i < ac_block_count; i++)
        {
            hi = ac_block[i].audio_channels;
            if (hi >= 0 && hi < MAX_AUDIO_CHANNELS)
            {
                ac_histogram[hi].frames += ac_block[i].end - ac_block[i].start + 1;
                ac_histogram[hi].audio_channels = ac_block[i].audio_channels;
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
            if (ac_histogram[i].frames < ac_histogram[i + 1].frames)
            {
                hadToSwap = true;
                tempFrames = ac_histogram[i].frames;
                tempAC  = ac_histogram[i].audio_channels;
                ac_histogram[i] = ac_histogram[i + 1];
                ac_histogram[i+1].frames = tempFrames;
                ac_histogram[i+1].audio_channels = tempAC;
            }
        }
    }
    while (hadToSwap);

    for (i = 0; i < MAX_AUDIO_CHANNELS; i++)
    {
        totalFrames += ac_histogram[i].frames;
    }

    tempCount = 0;
    Debug(10, "\n\nAfter Sorting - %i\n--------------\n", counter);
    i = 0;
    while (i < MAX_AUDIO_CHANNELS && ac_histogram[i].frames > 0)
    {
        tempCount += ac_histogram[i].frames;
        Debug(10, "Audio channels %3i found on %6i frames totalling \t%3.1f%c\n", ac_histogram[i].audio_channels, ac_histogram[i].frames, ((double)tempCount / (double)totalFrames)*100,'%');
        i++;
    }

}




void InsertBlackFrame(int f, int b, int u, int v, int c)
{
    int i;

    //		if ((black_count==0 || black[black_count-1].frame < logo_block[logo_block_count-1].end )) {

    i = 0;
    while (i < black_count && black[i].frame != f)
        i++;

    if (i < black_count && black[i].frame == f)
    {
        black[i].cause |= c;
    }
    else
    {
        if (black_count >= max_black_count)
        {
            max_black_count += 500;
            black = static_cast<black_frame_info *>( realloc(black, (max_black_count + 1) * sizeof(black_frame_info)) );
            Debug(9, "Resizing black frame array to accommodate %i frames.\n", max_black_count);
        }


        //	InitializeBlackArray(black_count);
        black_count++;
        i = black_count-2;
        while (i >= 0 && black[i].frame > f)
        {
            black[i+1] = black[i];
            i--;
        }
        i++;

        black[i].frame = f;
        black[i].brightness = b;
        black[i].uniform = u;
        black[i].volume = v;
        black[i].cause = c;
    }
}



bool BuildMasterCommList(void)
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
    int		platauHistogram[256];

    double	length;
    double	new_ar_ratio;
    FILE*	logo_file = NULL;
    bool	foundCommercials = false;
    time_t	ltime;

    if (frame_count == 0)
    {
        Debug(1, "No video found\n");
        return(false);
    }
    Debug(7, "Finished scanning file.  Starting to build Commercial List.\n");


//    if (fabs(avg_fps - fps)> 0.01)
//        Debug(1,"WARNING: Actual framerate (%6.3f) different from specified framerate (%6.3f)\n", avg_fps, fps);


    length = F2L(frame_count-1, 1);
    if (fabs( length - (frame_count -1)/fps) > 0.5) {
        if (fabs(avg_fps - fps)> 1)
            Debug(1,"WARNING: Actual framerate (%6.3f) different from specified framerate (%6.3f)\nInternal frame numbers will be different from .txt frame numbers\n", avg_fps, fps);
        Debug(1,"WARNING: Complex timeline or errors in the recording!!!!\nResults may be wrong, .ref input will be misaligned. .txt editing will produce wrong results\nUse .edl output if possible\n");
    }

    frame[frame_count].pts = frame[frame_count-1].pts + 1.0 / fps;


    for (i = 1; i < 255; i++)
    {
        if (volumeHistogram[i] > 10)
        {
            min_volume = (i-1)*volumeScale;
            break;
        }
    }

    for (k = 1; k < 255; k++)
    {
        if (uniformHistogram[k] > 10)
        {
            min_uniform = (k-1)*UNIFORMSCALE;
            break;
        }
    }

    for (i = 0; i < 255; i++)
    {
        if (brightHistogram[i] > 1)
        {
            min_brightness_found = i;
            break;
        }
    }


    logoPercentage = (double) frames_with_logo / (double) framenum_real;

//	if (max_volume == 0)
    {

#define VOLUME_DELTA	10
#define VOLUME_MAXIMUM	300
#define VOLUME_PLATAU_SIZE		6

        volume_delta = VOLUME_DELTA;

try_again:
        if (framearray)  			// Find silence volume level
        {

            for (i = 0; i < 255; i++)
                platauHistogram[i] = 0;
            plataus = 0;
            j = 1;
            for (i = VOLUME_PLATAU_SIZE; i < frame_count-VOLUME_PLATAU_SIZE;)
            {
                if (frame[i].volume > VOLUME_MAXIMUM || frame[i].volume < 0)
                {
                    i++;
                    continue;
                }
                while (i+1 < frame_count-VOLUME_PLATAU_SIZE && frame[i+1].volume < frame[i].volume)
                    i++;
                k = 1;
                while (i-k - VOLUME_PLATAU_SIZE > 1 &&
                        (abs(frame[i-k].volume - frame[i].volume) < volume_delta
                         //|| frame[i-k].volume < 50
                        ))
                {
                    k++;
                }
                if (frame[i-k].volume < frame[i].volume)
                {
                    i++;
                    continue;
                }
                a = 1;
                while (i+a +VOLUME_PLATAU_SIZE < frame_count &&
                        (abs(frame[i+a].volume - frame[i].volume) < volume_delta
                         //|| frame[i+a].volume < 50
                        ))
                {
                    a++;
                }
                if (frame[i+a].volume < frame[i].volume)
                {
                    i = i+a;
                    continue;
                }
// i=8 k=1 a=11
                if (a+k > VOLUME_PLATAU_SIZE && i-k-VOLUME_PLATAU_SIZE > 0 && i+a+VOLUME_PLATAU_SIZE < frame_count)
                {
                    p_vol = (frame[i-k-4].volume +
                             frame[i-k-VOLUME_PLATAU_SIZE+3].volume +
                             frame[i-k-VOLUME_PLATAU_SIZE+2].volume +
                             frame[i-k-VOLUME_PLATAU_SIZE+1].volume +
                             frame[i-k-VOLUME_PLATAU_SIZE].volume) / 5;
                    n_vol = (frame[i+a+4].volume +
                             frame[i+a+VOLUME_PLATAU_SIZE-3].volume +
                             frame[i+a+VOLUME_PLATAU_SIZE-2].volume +
                             frame[i+a+VOLUME_PLATAU_SIZE-1].volume +
                             frame[i+a+VOLUME_PLATAU_SIZE].volume) / 5;
                    if ( p_vol > frame[i].volume + 220 || n_vol > frame[i].volume + 220 )
                        //if ( abs(frame[i-k-2].volume - frame[i].volume) > VOLUME_DELTA*2 ||
                        //	abs(frame[i+a+2].volume - frame[i].volume) > VOLUME_DELTA*2)
                    {
                        Debug(8, "Platau@[%d] frames %d, volume %d, distance %d seconds\n", i, k+a, frame[i].volume, (int)F2L(i,j));
                        j = i;
//						for (j = i-k; j < i + a; j++)
//							frame[j].isblack |= C_v;

                        plataus++;
                        platauHistogram[frame[i].volume/10]++;
                    }
                }
                i += a;
            }
            a = 0;
            Debug(9, "Vol : #Frames\n");
            for (i = 0; i < 255; i++)
            {
                a += platauHistogram[i];
                if (platauHistogram[i] > 0)
                    Debug(9, "%3d : %d\n", i*10, platauHistogram[i]);

            }
            a = a * 6 / 10;
            j = 0;
            i = 0;
            while (j < a)
            {
                j += platauHistogram[i++];
            }
            ms = i*10;
            Debug(7, "Calculated silence level = %d\n", ms);
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
    if (max_volume == 0)
    {
        max_volume = mv;
        max_silence = ms;
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
    if (max_volume == 0)
    {

        if (framearray)  			// Find silence volume level
        {

#define START_VOLUME	500
            count = 21;
scanagain:
            a = START_VOLUME;
            k = 0;
            if (frame[i].volume > 0)
                j = frame[i].volume;
            else
                j = 0;
            for (i = 1; i < frame_count; i++ )
            {
                if (frame[i].volume > 0 && frame[i].volume < a)
                {
                    if (frame[i].volume < j)
                        j = frame[i].volume;
                    k++;
                    if (k > count && a > frame[i-count].volume + 20 &&
                            j > a - 250)
                    {
                        i = i - count;
                        a = frame[i].volume;
                        j = frame[i].volume;
                        k = 0;
                    }
                }
                else
                {
                    k = 0;
                    if (frame[i].volume > 0)
                        j = frame[i].volume;
                }
            }
        }
        if (a > START_VOLUME-100 && count > 7)
        {
            count = count - 7;
            goto scanagain;
        }
        max_silence = a+10;
        max_volume = a+150;
    }

    if (max_volume == 0)
    {

        for (k = 2; k < 255; k++)
        {
            if (volumeHistogram[k] > 10)
            {
                max_volume = k*volumeScale + 200;
                max_silence = k*volumeScale + 20;
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
        Debug ( 1, "Setting max_volume to %i\n", max_volume);
    }

    if (commDetectMethod & LOGO)
    {
        // close out last logo cblock if one is open
        ProcessLogoTest(frame_count, false, true);
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

        if (logo_quality == 0.0)
            FindLogoThreshold();

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
        for (i = logo_block_count-1; i >= 0; i--)
        {
            if (F2L(logo_block[i].end, logo_block[i].start) < min_commercial_size - 2*shrink_logo)
            {
                Debug(1, "Logo cblock %d deleted because too short (%i s)\n", i, (int)F2L(logo_block[i].end, logo_block[i].start) );
                for (t = i; t+1 < logo_block_count; t++)
                {
                    logo_block[t] = logo_block[t+1];
                }
                logo_block_count--;
            }
        }
        if (logoPercentage > logo_fraction && after_logo > 0 && framearray && logo_block_count > 0)
        {
            for (i = 0; i < logo_block_count; i++)
            {
                if (i < logo_block_count-1 && F2L(logo_block[i+1].start, logo_block[i].end)< max_commercialbreak/4)
                    continue;			// Don't do anything if too close
                if (i == logo_block_count-1 && F2L(frame_count, logo_block[i].end) < max_commercialbreak/4)
                    continue;			// Don't do anything if too close
                if (after_logo==999)
                {
                    j = logo_block[i].end;
                    InsertBlackFrame(j,frame[j].brightness,frame[j].uniform,0, C_l);
                    Debug(
                        3,
                        "Frame %6i (%.3fs) - Cutpoint added when Logo disappears\n",
                        get_frame_pts(j),
                        j
                    );
                    continue;
                }

                j = logo_block[i].end + (int)(after_logo * fps);
                if ( j >= frame_count)
                    j = frame_count-1;
                t = j + (int)(30 * fps);
                if ( t >= frame_count)
                    t = frame_count-1;
                maxsc = 255;
                cp = 0;
                cpf = 0;
                while (j < t)
                {
                    rsc = 255;
                    while (frame[j].volume >= max_volume && j < t)
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
                        if (rsc > frame[j].schange_percent)
                        {
                            rsc = frame[j].schange_percent;
                            cpf = j;
                        }
                        j++;
                    }
                    if (j == t )
                        break;
                    while (frame[j].volume < max_volume && j < t)
                    {
                        if (rsc > frame[j].schange_percent)
                        {
                            rsc = frame[j].schange_percent;
                            cpf = j;
                        }
                        j++;
                    }
                    if (j == t )
                        break;
                    c = 10;
                    while (c-- && j < t)
                    {
                        if (rsc > frame[j].schange_percent)
                        {
                            rsc = frame[j].schange_percent;
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
                    InsertBlackFrame(cp,frame[cp].brightness,frame[cp].uniform,frame[cp].volume, C_l);
                    Debug(
                        3,
                        "Frame %6i (%.3fs) - Cutpoint added %i seconds after Logo disappears at change percentage of %d\n",
                        cp, get_frame_pts(cp), (int)F2L(cp, logo_block[i].end), maxsc
                    );
                }
            }
        }

        if (logoPercentage > logo_fraction && before_logo > 0 && framearray && logo_block_count > 0)
        {
            for (i = 0; i < logo_block_count; i++)
            {
                if (i > 0 && F2L(logo_block[i].start, logo_block[i-1].end) < max_commercialbreak/4)
                    continue;
                if (i == 0 && F2T(logo_block[i].start) < max_commercialbreak/4)
                    continue;
                if (before_logo==999)
                {
                    j = logo_block[i].start;
                    InsertBlackFrame(j,frame[j].brightness,frame[j].uniform,0, C_l);
                    Debug(
                        3,
                        "Frame %6i (%.3fs) - Cutpoint added when Logo appears\n",
                        j, get_frame_pts(j)
                    );

                    continue;
                }

                j = logo_block[i].start - (int)(before_logo * fps);
                if ( j < 1)
                    j = 1;
                t = j - (int)(30 * fps);
                if ( t < 1)
                    t = 1;
                maxsc = 255;
                cp = 0;
                cpf = 0;
                while (j > t)
                {
                    rsc = 255;
                    while (frame[j].volume >= max_volume && j > t) // Search low volume
                    {
//						if (rsc > frame[j].schange_percent)
//							rsc = frame[j].schange_percent;
                        j--;
                    }
                    if (j == t )
                        break;
                    c = 10;
                    j = j + 10;
                    if (j >= frame_count)
                    {
                        j = j - frame_count;
                        c = c - j;
                        j = frame_count - 1;
                    }
                    while (c-- && j > t) // Largest scene change 10 frames before low volume
                    {
                        if (rsc > frame[j].schange_percent)
                        {
                            rsc = frame[j].schange_percent;
                            cpf = j;
                        }
                        j--;
                    }
                    if (j == t )
                        break;

                    while (frame[j].volume < max_volume && j > t) // largest scene change in low volume
                    {
                        if (rsc > frame[j].schange_percent)
                        {
                            rsc = frame[j].schange_percent;
                            cpf = j;
                        }
                        j--;
                    }
                    if (j == t )
                        break;
                    c = 10;
                    while (c-- && j > t) //Largest scene change after low volume
                    {
                        if (rsc > frame[j].schange_percent)
                        {
                            rsc = frame[j].schange_percent;
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
                    InsertBlackFrame(cp,frame[cp].brightness,frame[cp].uniform,frame[cp].volume, C_l);
                    Debug(
                        3,
                        "Frame %6i (%.3fs) - Cutpoint added %i seconds before Logo appears at change percentage of %d\n",
                        cp, get_frame_pts(cp), (int)F2L(logo_block[i].start, cp), maxsc
                    );
                }
            }
        }
//		if (logoPercentage > .15 && logoPercentage < .32 ) {
//			reverseLogoLogic = true;
//			logoPercentage = 1 - logoPercentage;
//		}
        if (logoPercentage < logo_fraction - 0.05 || logoPercentage > logo_percentile)
        {
            Debug(1, "\nNot enough or too much logo's found (%.2f), disabling the use of Logo detection\n",logoPercentage );
            commDetectMethod -= LOGO;
        }
    }

    if (remove_silent_segments) {
        i = 1;
        j = 1;
        for (i=1; i < frame_count; i++)
        {
            if (frame[i].volume < 5) {
                j = i+1;
                while (j < frame_count && frame[j].volume < 10 ) j++;
                if ((frame[j-1].pts - frame[i].pts) > remove_silent_segments) {
                    Debug(4, "\nDetected a long silent segment from frames %d till %d\n", i, j-1);
                    InsertBlackFrame(i,frame[i].brightness,frame[i].uniform,frame[i].volume, C_v);
                    InsertBlackFrame(j-1,frame[j-1].brightness,frame[j-1].uniform,frame[j].volume, C_v);
                }
                i = j + 1;
            }
        }
    }

    if (commDetectMethod & SILENCE)
    {
        silence_count = 0;
        schange_found = false;
        schange_frame = 0;
        schange_max = 100;
        low_volume_count = 0;
        very_low_volume_count = 0;
        for (i=1; i <frame_count; i++)
        {
            if (frame[i].volume < 6)
            {
                InsertBlackFrame(i,frame[i].brightness,frame[i].uniform,frame[i].volume, C_v);
            } else
            if (min_silence > 0)
            {
                if (0 <= frame[i].volume && frame[i].volume < max_silence)
                {
                    if (silence_start == 0)
                        silence_start = i;
                    silence_count++;
                    if (frame[i].schange_percent < schange_threshold)
                    {
                        schange_found = true;
                        if (schange_max > frame[i].schange_percent)
                        {
                            schange_frame = i;
                            schange_max = frame[i].schange_percent;
                        }
                    }
                    if (frame[i].uniform < non_uniformity)
                    {
                        schange_found = true;
                        schange_frame = i;
                    }
                    if (frame[i].volume < max_silence)
                    {
                        low_volume_count++;
                    }
                    if (frame[i].volume < 9)
                    {
                        very_low_volume_count++;
                    }
                }
                else
                {
                    if (silence_count > min_silence /* * (int)fps */ && silence_count < 5 * fps)
                    {

                        if ( very_low_volume_count > (int)(silence_count * 0.7) ||  schange_found || frame[i].schange_percent < schange_threshold)
                        {
#define SILENCE_CHECK	((int)(2.5 * fps))
                            summed_volume1 = 0;
                            for (j = max(silence_start - SILENCE_CHECK,1); j < silence_start; j++)
                            {
//							if (summed_volume1 < frame[j].volume)
                                summed_volume1 += frame[j].volume;
                            }
                            summed_volume1 /= min(SILENCE_CHECK, silence_start+1) ;
                            summed_volume2 = 0;
                            for (j = i; j < min(i+SILENCE_CHECK, frame_count); j++)
                            {
//							if (summed_volume2 < frame[j].volume)
                                summed_volume2 += frame[j].volume;
                            }
                            summed_volume2 /= min(SILENCE_CHECK, frame_count - i + 1);
                            if ((summed_volume1 > 0.9*max_volume &&  summed_volume2 > 0.9*max_volume && low_volume_count > min_silence ) ||
                                    (summed_volume1 > 2*max_volume &&  summed_volume2 > 2*max_volume) ||
                                    (summed_volume1 > 4*max_volume ||  summed_volume2 > 4*max_volume) ||
                                    very_low_volume_count  > min_silence
                               )
                            {
                                if (schange_frame == 0)
                                    schange_frame = i;

#if 1
                                for (j=silence_start; j < i; j++)
                                {
                                    frame[j].isblack |= C_v;
                                    InsertBlackFrame(j,frame[j].brightness,frame[j].uniform,frame[j].volume, C_v);
                                }
#else
                                frame[schange_frame].isblack |= C_v;
                                InsertBlackFrame(schange_frame,frame[schange_frame].brightness,frame[schange_frame].uniform,frame[schange_frame].volume, C_v);
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


    after_start = added_recording * fps * 60;
    before_end  = frame_count - added_recording * fps * 60;

    frame[frame_count].dimCount = 0;
    frame[frame_count].hasBright = 0;
    InsertBlackFrame(frame_count,0,0,0, C_b);

    if (cut_on_ac_change)
    {
        if (ac_block[ac_block_count].start > 0)
        {
            Debug(5, "The last ar cblock wasn't closed.  Now closing.\n");
            ac_block[ac_block_count].end = frame_count;
            ac_block_count++;
        }

        FillACHistogram(true);
        dominant_ac = ac_histogram[0].audio_channels;

        // Print out ar cblock list
        Debug(4, "\nPrinting AC cblock list\n-----------------------------------------\n");
        for (i = 0; i < ac_block_count; i++)
        {
            Debug(
                4,
                "Block: %i\tStart: %6i\tEnd: %6i\taudio channels: %2i\tLength: %s\n",
                i,
                ac_block[i].start,
                ac_block[i].end,
                ac_block[i].audio_channels,
                dblSecondsToStrMinutes(F2L(ac_block[i].end, ac_block[i].start) )
            );
        }
    }

    // close out the last ar cblock
    if (commDetectMethod & AR)
    {
        if (ar_block[ar_block_count].start > 0)
        {
            Debug(5, "The last ar cblock wasn't closed.  Now closing.\n");
            ar_block[ar_block_count].end = frame_count;
            ar_block_count++;
        }


        // Print out ar cblock list
        Debug(9, "\nPrinting AR cblock list before cleaning\n-----------------------------------------\n");
        for (i = 0; i < ar_block_count; i++)
        {
            Debug(
                9,
                "Block: %i\tStart: %6i\tEnd: %6i\tAR_R: %.2f\tLength: %s, [%4dx%4d] minX=%3d, minY=%3d, maxX=%3d, maxY=%3d\n",
                i,
                ar_block[i].start,
                ar_block[i].end,
                ar_block[i].ar_ratio,
                dblSecondsToStrMinutes(F2L(ar_block[i].end, ar_block[i].start) ),
                ar_block[i].width, ar_block[i].height,
                ar_block[i].minX, ar_block[i].minY, ar_block[i].maxX, ar_block[i].maxY
            );
        }

        // Calculate histogram with noisy aspect ratios
        FillARHistogram(false);

        // Update histogram to remove replaced ratios
        for (i = 0 ; i < MAX_ASPECT_RATIOS; i++)
        {
            for (j = i+1; j < MAX_ASPECT_RATIOS; j++)
            {
                if (ar_histogram[j].ar_ratio < ar_histogram[i].ar_ratio+ar_delta &&
                        ar_histogram[j].ar_ratio > ar_histogram[i].ar_ratio-ar_delta )
                    ar_histogram[j].ar_ratio = ar_histogram[i].ar_ratio;
            }

        }

        // Normalize aspect ratios
        for (i = 0; i < ar_block_count; i++)
        {
            new_ar_ratio = FindARFromHistogram (ar_block[i].ar_ratio);
            ar_block[i].ar_ratio = new_ar_ratio;
        }
        // Calculate histogram with normalized aspect ratios
        FillARHistogram(true);
        dominant_ar = ar_histogram[0].ar_ratio;


again:
        // Clean up ar cblock list

        for (i = ar_block_count - 1; i > 0; i--)
        {
            length = ar_block[i].end - ar_block[i].start;

            if (cut_on_ar_change > 2 && length < cut_on_ar_change*(int)fps && ar_block[i].ar_ratio != AR_UNDEF )
            {
                Debug(
                    6,
                    "Undefining AR cblock %i because it is too short\n",
                    i,
                    dblSecondsToStrMinutes(length / fps)
                );
                ar_block[i].ar_ratio = AR_UNDEF;
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
            if (commDetectMethod & LOGO && 	ar_block[i - 1].ar_ratio != AR_UNDEF &&
                    ar_block[i].ar_ratio > ar_block[i - 1].ar_ratio &&
                    CheckFrameForLogo(ar_block[i-1].end) &&
                    CheckFrameForLogo(ar_block[i].start) )
            {
                if (ar_block[i].end - ar_block[i].start > ar_block[i-1].end - ar_block[i-1].start)
                {
                    j = ar_block[i-1].start;
                    ar_block[i-1] = ar_block[i];
                    ar_block[i-1].start = j;
                }
                else
                    ar_block[i - 1].end = ar_block[i].end;
                ar_block_count--;
                Debug(
                    6,
                    "Joining AR blocks %i and %i because both have logo\n",
                    i - 1,
                    i,
                    i,
                    dblSecondsToStrMinutes(length / fps)
                );
                for (j = i; j < ar_block_count; j++)
                {
                    ar_block[j] = ar_block[j + 1];
                }
                goto again;
            }
//
#endif
            if ( i == 1 && ar_block[i-1].ar_ratio == AR_UNDEF)
            {
                j = ar_block[i - 1].start;
                ar_block[i - 1] = ar_block[i];
                ar_block[i - 1].start = j;
                ar_block_count--;
                Debug(6, "Joining AR blocks %i and %i because cblock 0 has an AR ratio of 0.0\n", i - 1, i, ar_block[i-1].ar_ratio);
                for (j = i; j < ar_block_count; j++)
                {
                    ar_block[j] = ar_block[j + 1];
                }
                goto again;

            }
            if (( ar_block[i].ar_ratio - ar_block[i - 1].ar_ratio < ar_delta &&
                    ar_block[i].ar_ratio - ar_block[i - 1].ar_ratio > -ar_delta ))
            {
                ar_block[i - 1].end = ar_block[i].end;
                ar_block_count--;
                Debug(6, "Joining AR blocks %i and %i because both have an AR ratio of %.2f\n", i - 1, i, ar_block[i].ar_ratio);
                for (j = i; j < ar_block_count; j++)
                {
                    ar_block[j] = ar_block[j + 1];
                }
                goto again;

            }
            if (  ar_block[i-1].ar_ratio == AR_UNDEF && i > 1 &&
                    ar_block[i].ar_ratio - ar_block[i - 2].ar_ratio < ar_delta &&
                    ar_block[i].ar_ratio - ar_block[i - 2].ar_ratio > -ar_delta )
            {
                ar_block[i - 2].end = ar_block[i].end;
                ar_block_count -= 2;
                Debug(6, "Joining AR blocks %i and %i because they have a dummy cblock inbetween\n", i - 2, i);
                for (j = i-1; j < ar_block_count; j++)
                {
                    ar_block[j] = ar_block[j + 2];
                }
                goto again;

            }
        }

        // Print out ar cblock list
        Debug(4, "\nPrinting AR cblock list\n-----------------------------------------\n");
        for (i = 0; i < ar_block_count; i++)
        {
            Debug(
                4,
                "Block: %i\tStart: %6i\tEnd: %6i\tAR_R: %.2f\tLength: %s, [%4dx%4d] minX=%3d, minY=%3d, maxX=%3d, maxY=%3d\n",
                i,
                ar_block[i].start,
                ar_block[i].end,
                ar_block[i].ar_ratio,
                dblSecondsToStrMinutes(F2L(ar_block[i].end, ar_block[i].start) ),
                ar_block[i].width, ar_block[i].height,
                ar_block[i].minX, ar_block[i].minY, ar_block[i].maxX, ar_block[i].maxY
            );
        }
    }

    // close out the last cc cblock
    if (processCC)
    {
        cc_block[cc_block_count].end_frame = frame_count;
        cc_block_count++;
        cc_text[cc_text_count].end_frame = frame_count;
        cc_text_count++;
        for (i = cc_text_count - 1; i > 0; i--)
        {
            if (cc_text[i].text_len == 0)
            {
                for (j = i; j < cc_text_count; j++)
                {
                    cc_text[j].start_frame = cc_text[j + 1].start_frame;
                    cc_text[j].end_frame = cc_text[j + 1].end_frame;
                    cc_text[j].text_len = cc_text[j + 1].text_len;
                    strncpy((char*)cc_text[j].text, (char*)cc_text[j + 1].text, sizeof(cc_text[j].text));
                }

                cc_text_count--;
            }
        }

        Debug(2, "Closed caption transcript\n--------------------\n");

        for (i = 0; i < cc_text_count; i++)
        {
            Debug(
                2,
                "%i) S:%6i E:%6i L:%4i %s\n",
                i,
                cc_text[i].start_frame,
                cc_text[i].end_frame,
                cc_text[i].text_len,
                cc_text[i].text
            );
        }
    }

    if (output_framearray) OutputFrameArray(false);
    if (output_framearray) OutputBlackArray();

    BuildBlocks(false);
    if (commDetectMethod & LOGO)
    {
        PrintLogoFrameGroups();
    }
    WeighBlocks();

    foundCommercials = OutputBlocks();

    if (verbose)
    {
        Debug(1, "\n%i Frames Processed\n", framesprocessed);
        log_file = myfopen(logfilename, "a+");
        fprintf(log_file, "################################################################\n");
        time(&ltime);
        fprintf(log_file, "Time at end of run:\n%s", ctime(&ltime));
        fprintf(log_file, "################################################################\n");
        fclose(log_file);
        log_file = NULL;
    }


    if (ccCheck && processCC)
    {
        char temp[MAX_PATH];
        FILE* tempFile;
        if ((most_cc_type == PAINTON) || (most_cc_type == ROLLUP) || (most_cc_type == POPON))
        {
            sprintf(temp, "%s.ccyes", workbasename);
            tempFile = myfopen(temp, "w");
            fclose(tempFile);
            sprintf(temp, "%s.ccno", workbasename);
            myremove(temp);
        }
        else
        {
            sprintf(temp, "%s.ccno", workbasename);
            tempFile = myfopen(temp, "w");
            fclose(tempFile);
            sprintf(temp, "%s.ccyes", workbasename);
            myremove(temp);
        }
    }

    if (deleteLogoFile)
    {
        logo_file = myfopen(logofilename, "r");
        if(logo_file)
        {
            fclose(logo_file);
            myremove(logofilename);
        }
    }

//	free(frame);
    return (foundCommercials);
}
