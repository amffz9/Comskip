#include "legacy_detection.h"

void ProcessARInfoInit(int minY, int maxY, int minX, int maxX)
{
    double pictureHeight = maxY - minY;
    double pictureWidth = maxX - minX;

    if (minX <= border) minX = 1;
    if (minY <= border) minY = 1;
    if (maxY >= height - border) maxY = height;
    if (maxX >= videowidth - border) maxX = videowidth;

    /*
    ar_width = width;
    if (ar_width < maxY + minY)
    	ar_width = (int)((maxY + minY) * 1.3);
    */

    last_ar_ratio = (double)(pictureWidth) / (double)pictureHeight;
    last_ar_ratio = ceil(last_ar_ratio * ar_rounding) / ar_rounding;
    ar_ratio_trend = last_ar_ratio;
    if (last_ar_ratio < 0.5 || last_ar_ratio > 3.0)
        last_ar_ratio = AR_UNDEF;
//	lastAR = (last_ar_ratio <= ar_split);
    ar_ratio_start = framenum_real;
    ar_block[ar_block_count].start = framenum_real;
    ar_block[ar_block_count].width = videowidth;
    ar_block[ar_block_count].height = height;
    ar_block[ar_block_count].minX = minX;
    ar_block[ar_block_count].minY = minY;
    ar_block[ar_block_count].maxX = maxX;
    ar_block[ar_block_count].maxY = maxY;
//			ar_block[ar_block_count].ar = lastAR;
    ar_block[ar_block_count].ar_ratio = last_ar_ratio;
//	if (framearray) frame[frame_count].ar_ratio = last_ar_ratio;;
    Debug(4, "Frame: %i\tRatio: %.2f\tMinY: %i MaxY: %i MinX: %i MaxX: %i\n", ar_ratio_start, ar_ratio_trend , minY, maxY, minX, maxX);

//	Debug(4, "\nFirst Frame\nFrame: %i\tMinY: %i\tMaxY: %i\tRatio: %.2f\n", framenum_real, minY, maxY, last_ar_ratio);
}

void ProcessARInfo(int minY, int maxY, int minX, int maxX)
{
    int		pictureHeight, pictureWidth;
    int		hi,i;
    double	cur_ar_ratio;

    if (minX <= border) minX = 1;
    if (minY <= border) minY = 1;
    if (maxY >= height - border) maxY = height;
    if (maxX >= videowidth - border) maxX = videowidth;


    if (ticker_tape_percentage>0)
        ticker_tape = ticker_tape_percentage * height / 100;
    if (top_ticker_tape_percentage>0)
        top_ticker_tape = top_ticker_tape_percentage * height / 100;
    if (ticker_tape != 0 || top_ticker_tape != 0 || (
                abs((height - maxY) - (minY)) < 13 + (minY )/15  &&  // discard for no simetrical check
                abs((videowidth  - maxX) - (minX)) < 13 + (minX )/15  &&  // discard for no simetrical check
                minY < height / 4 &&
                minX < videowidth / 4)
       )   // check if simetrical
    {

        pictureHeight = maxY - minY;
        pictureWidth = maxX - minX;
        cur_ar_ratio = (double)(pictureWidth) / (double)pictureHeight;
        cur_ar_ratio = ceil(cur_ar_ratio * ar_rounding) / ar_rounding;
        if (cur_ar_ratio > 3.0 || cur_ar_ratio < 0.5)
            cur_ar_ratio = AR_UNDEF;

        hi = (int)((cur_ar_ratio - 0.5)*100);
        if (hi >= 0 && hi < MAX_ASPECT_RATIOS)
        {
            ar_histogram[hi].frames += 1;
            ar_histogram[hi].ar_ratio = cur_ar_ratio;
        }


        if (cur_ar_ratio - last_ar_ratio > ar_delta || cur_ar_ratio - last_ar_ratio < -ar_delta)
        {
            if (cur_ar_ratio - ar_ratio_trend < ar_delta && cur_ar_ratio - ar_ratio_trend > -ar_delta)
            {
                // Same ratio as previous trend
                ar_ratio_trend_counter++;
                if (ar_ratio_trend_counter / fps > AR_TREND)
                {
                    last_ar_ratio = ar_ratio_trend;
                    ar_ratio_trend_counter = 0;
                    ar_misratio_trend_counter = 0;
                    if (commDetectMethod & AR)
                    {

                        ar_block[ar_block_count].end = ar_ratio_start-1;
                        ar_block_count++;
                        InitializeARBlockArray(ar_block_count);
                        ar_block[ar_block_count].start = ar_ratio_start;
                        ar_block[ar_block_count].ar_ratio = ar_ratio_trend;
                        ar_block[ar_block_count].volume = 0;
                        ar_block[ar_block_count].width = videowidth;
                        ar_block[ar_block_count].height = height;
                        ar_block[ar_block_count].minX = minX;
                        ar_block[ar_block_count].minY = minY;
                        ar_block[ar_block_count].maxX = maxX;
                        ar_block[ar_block_count].maxY = maxY;
                    }
                    Debug(9, "Frame: %i\tRatio: %.2f\tMinY: %i\tMaxY: %i\tMinX: %i\tMaxX: %i\n", ar_ratio_start, ar_ratio_trend , minY, maxY, minX, maxX);
                    last_ar_ratio = ar_ratio_trend;
                    if (framearray)
                    {
                        for (i = ar_ratio_start; i < framenum_real; i++)
                        {
                            frame[i].ar_ratio = last_ar_ratio;
                        }
                    }
                }
            }
            else  						// Other ratio as previous trend
            {
                ar_ratio_trend = cur_ar_ratio;
                ar_ratio_trend_counter = 0;
                ar_ratio_start = framenum_real;
            }
            ar_misratio_trend_counter++;
        }
        else  							// Same ratio as previous frame
        {
            ar_ratio_trend_counter = 0;
            ar_ratio_start = framenum_real;
            ar_misratio_trend_counter = 0;
            ar_misratio_start = framenum_real;
        }

    }
    else
    {
        // Unreliable ratio
//		ar_ratio_trend = cur_ar_ratio;
        ar_ratio_trend_counter = 0;
        ar_ratio_start = framenum_real;
        /*
          //	if (framearray) frame[frame_count].minY = 0;
        	//	if (framearray) frame[frame_count].maxY = height;
        	//	Debug(9, "Frame: %i\tAsimetrical\tMinY: %i\tMaxY: %i\n", framenum_real, minY, maxY);
        		ar_ratio_trend_counter = 0;
        		ar_ratio_start = framenum_real;
        		ar_misratio_trend_counter++;
        		if (framearray) frame[frame_count].ar_ratio = 0.0;
        */

    }
    /*
    	if (last_ar_ratio == 0) {
    		ar_misratio_trend_counter = 0;

    	} else {
    */
    if (ar_misratio_trend_counter > 3*fps && last_ar_ratio != AR_UNDEF)
    {
        last_ar_ratio = ar_ratio_trend = AR_UNDEF;
        if (commDetectMethod & AR)
        {
            ar_block[ar_block_count].end = framenum_real - 3*(int)fps -1;
            ar_block_count++;
            InitializeARBlockArray(ar_block_count);
            ar_block[ar_block_count].start = framenum_real - 3*(int)fps;
            ar_block[ar_block_count].ar_ratio = ar_ratio_trend;
            ar_block[ar_block_count].volume = 0;
            ar_block[ar_block_count].width = videowidth;
            ar_block[ar_block_count].height = height;
            ar_block[ar_block_count].minX = minX;
            ar_block[ar_block_count].minY = minY;
            ar_block[ar_block_count].maxX = maxX;
            ar_block[ar_block_count].maxY = maxY;
        }
        Debug(9, "Frame: %i\tRatio: %.2f\tMinY: %i\tMaxY: %i\n", ar_ratio_start, ar_ratio_trend , minY, maxY);
        last_ar_ratio = ar_ratio_trend;
    }
//	}

}

void ProcessACInfoInit(int audio_channels)
{
//    audio_channels_start = framenum_real;
    ac_block[ac_block_count].start = framenum_real;
    ac_block[ac_block_count].audio_channels = audio_channels;
    last_audio_channels = audio_channels;
    Debug(4, "Frame: %i Channels: %2i\n", framenum_real, audio_channels);
}

void ProcessACInfo(int audio_channels)
{
    if (last_audio_channels == audio_channels )
        return;
    ac_block[ac_block_count].end = framenum_real;
    ac_block_count++;
    InitializeACBlockArray(ac_block_count);
    last_audio_channels = audio_channels;
//    audio_channels_start = framenum_real;
    ac_block[ac_block_count].start = framenum_real;
    ac_block[ac_block_count].audio_channels = audio_channels;
    last_audio_channels = audio_channels;
    Debug(4, "Frame: %i Channels: %2i\n", framenum_real, audio_channels);
}

int MatchCutScene(unsigned char *cutscene)
{
    int x,y,d;
    int delta = 0;
    int step = 4;
    int c=0;
    if (width > 800) step = 8;
    if (width < 400) step = 2;
    if (width < 200) step = 1;
    for (y = border; y < (height - border); y += step)
    {
        for (x = border; x < (videowidth - border); x += step)
        {
            if (c < MAXCSLENGTH)
            {
                d = (int)frame_ptr[y * width + x] - (int)(cutscene[c]);
                if (d > edge_level_threshold || d < -edge_level_threshold)
                    delta += 1;
            }
            c++;
        }
    }
    return(delta);
}

void RecordCutScene(int frame_count, int brightness)
{
    char cs[MAXCSLENGTH];
    int c;
    int x,y;
    int step = 4;

    if (width > 800) step = 8;
    if (width < 400) step = 2;
    if (width < 200) step = 1;
    c = 0;
    for (y = border; y < (height - border); y += step)
    {
        for (x = border; x < (videowidth - border); x += step)
        {
            if (c < MAXCSLENGTH)
            {
                cs[c++] = frame_ptr[y * width + x];
            }
        }
    }
    cutscene_file = NULL;
//GetDumpFileName();
    if (osname[0])
    {
        strcpy(cutscenefile, osname);
        strcat(cutscenefile, ".dmp");
    }
    if (cutscenefile[0] == 0)
    {
        sprintf(cutscenefile, "%s.dmp", workbasename);
    }
    if (cutscenefile[0])
    {
        cutscene_file = myfopen(cutscenefile,"wb");
    }
    if (cutscene_file != NULL)
    {
        fwrite(&brightness, sizeof(int), 1, cutscene_file);
        fwrite(cs, sizeof(char), c, cutscene_file);
        Debug(7, "Saved frame %6i into cutfile \"%s\"\n", frame_count, cutscenefile);
        fclose(cutscene_file);
        cutscene_file = NULL;
    }
}

void LoadCutScene(const char *filename)
{
    int i,j,b,c;
    cutscene_file = myfopen(filename,"rb");
    if (cutscene_file != NULL)
    {
        i = cutscenes;
        fread(&csbrightness[i], sizeof(int), 1, cutscene_file);
        c =	fread(cutscene[i], sizeof(char), MAXCSLENGTH, cutscene_file);
        if (c > 0)
        {
            Debug(7, "Loaded %i bytes from cutfile \"%s\"\n", c, filename);
            cslength[i] = c;
            b = 0;
            for (j = 0; j < c; j++)
                b += cutscene[i][j];
            // csbrightness[i] = b/c;
            cutscenes++;
        }
        else
        {
            Debug(1, "ERROR: Loading from cutfile \"%s\" failed\n", c, filename);
        }
        fclose(cutscene_file);
    } else
         Debug(1, "Can't open cutfile \"%s\"\n", filename);
}

#define OWN_HISTOGRAM_WIDTH 4
#define OWN_HISTOGRAM_HEIGHT 256

int own_histogram[OWN_HISTOGRAM_WIDTH][OWN_HISTOGRAM_HEIGHT];
int scan_step;

void ScanBottom(intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    int     w = (int) arg;
    brightCount = 0;
    max_delta =  min(videowidth,height)/2 - border;
    delta = 0;
    while (delta < max_delta)
    {
        y = border + delta;
        x = border + delta;
        i = y * width + x;
        i_max = y * width + videowidth - border - delta;
        i_step = scan_step;
        for (; i < i_max; i += i_step)
        {
            if (haslogo[i])
                continue;
            hereBright = frame_ptr[i];
#ifdef DEBUG_HERE_BRIGHT_MEM
            if (hereBright >= OWN_HISTOGRAM_HEIGHT) {
            	printf("Error, invalid here bright %i >= %i", hereBright, OWN_HISTOGRAM_HEIGHT);
            	exit(1);
            }
#endif
            own_histogram[0][hereBright]++;
            if (hereBright > test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            //brightCountminY = 0;
            minY = y;
        }
        delta += scan_step;
    }
}

void ScanTop(intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    int     w = (int) arg;

    max_delta =  min(videowidth,height)/2 - border;
    brightCount = 0;
    delta = 0;
    while (delta < max_delta)
    {
        x = border + delta;
        y = height - border - delta;
        i = y * width + x;
        i_max = y * width + videowidth - border - delta;
        i_step = scan_step;
        for (; i < i_max; i += i_step)
        {
            if (haslogo[i])
                continue;
            hereBright = frame_ptr[i];
#ifdef DEBUG_HERE_BRIGHT_MEM
            if (hereBright >= OWN_HISTOGRAM_HEIGHT) {
            	printf("Error, invalid here bright %i >= %i", hereBright, OWN_HISTOGRAM_HEIGHT);
            	exit(1);
            }
#endif
            own_histogram[1][hereBright]++;
            if (hereBright > test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            //brightCountmaxY = 0;
            maxY = y;
        }
        delta += scan_step;
    }
}

void ScanLeft(intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    int     w = (int) arg;

    max_delta =  min(videowidth,height)/2 - border;
    brightCount = 0;
    delta = 0;
    while (delta < max_delta)
    {
        x = border + delta;
        y = border + delta;
        i = y * width + x;
        i_step = scan_step * width;
        i_max = (height - border - delta) * width + x;
        for (; i< i_max; i += i_step)
        {
            if (haslogo[i])
                continue;
            hereBright = frame_ptr[i];
#ifdef DEBUG_HERE_BRIGHT_MEM
            if (hereBright >= OWN_HISTOGRAM_HEIGHT) {
            	printf("Error, invalid here bright %i >= %i", hereBright, OWN_HISTOGRAM_HEIGHT);
            	exit(1);
            }
#endif
            own_histogram[2][hereBright]++;
            if (hereBright > test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            //brightCountminX = 0;
            minX = x;
        }
        delta += scan_step;
    }
}

void ScanRight(intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    int     w = (int) arg;

    max_delta =  min(videowidth,height)/2 - border;
    brightCount = 0;
    delta = 0;
    while (delta < max_delta)
    {
        x = videowidth - border - delta;
        y = border + delta;
        i = y * width + x;
        i_step = scan_step * width;
        i_max = (height - border - delta) * width + x;
        for (; i < i_max; i += i_step)
        {
            if (haslogo[i])
                continue;
            hereBright = frame_ptr[i];
#ifdef DEBUG_HERE_BRIGHT_MEM
            if (hereBright >= OWN_HISTOGRAM_HEIGHT) {
            	printf("Error, invalid here bright %i >= %i", hereBright, OWN_HISTOGRAM_HEIGHT);
            	exit(1);
            }
#endif
            own_histogram[3][hereBright]++;
            if (hereBright > test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            maxX = x;
        }
        delta += scan_step;
    }
}

void DetectCredits(int frame_count)
{
static int credit_length = 0;
static int prev_credit_length = 0;
static int prev_credit_end = 0;
static int credit_count = 0;
        if (frame_count > 1 &&
            abs(frame[frame_count].brightness - frame[frame_count-1]. brightness) < 2  &&
            frame[frame_count].brightness < max_avg_brightness + 5
            ) {
                frame[frame_count].cutscenematch = frame[frame_count-1].cutscenematch - 1;
                credit_length++;

        }
        else if ( credit_length > fps * 0.5)
        {
            frame[frame_count].cutscenematch = 100;
            if (abs(credit_length - prev_credit_length)< 10 &&
                  frame_count - credit_length - prev_credit_end < (int)fps/2) {
                credit_count++;
                if (credit_count > 5)
                    Debug(1,"Credits[%i] detected at frame %i\n",credit_count,frame_count);
            }
            prev_credit_end = frame_count;
            prev_credit_length = credit_length;
            credit_length = 0;
        } else if (frame_count - prev_credit_end  < (int)fps/2) {
            frame[frame_count].cutscenematch = 100;
            credit_length = 0;
        } else {
            frame[frame_count].cutscenematch = 100;
            credit_length = 0;
            credit_count = 0;
            prev_credit_length = 0;
            prev_credit_end = 0;
        }

}


bool CheckSceneHasChanged(void)
{
    int		i;
    int		x;
    int		step;
    long	similar = 0;
//    static long prevsimilar = 0;
    int		hasBright = 0;
    int		dimCount = 0;
    bool	isDim = false;
    int pixels = 0;
//    int		brightCountminX;
//    int		brightCountminY;
//    int		brightCountmaxX;
//    int		brightCountmaxY;
    long	cause;
    int  uniform = 0;
    double scale = 1.0;

    if (!videowidth || !width || !height) return (false);
    minY = border;
    maxY = height - border;
    minX = border;
    maxX = videowidth - border;
    step = 2;
    if (videowidth > 1200) step = 3;
    if (videowidth > 1800) step = 4;
    if (videowidth < 600) step = 1;
    scan_step = step;

    if (edge_step == 0)
        edge_step = step; // Automatic adjust edge step for video size

    memcpy(lastHistogram, histogram, sizeof(histogram));
    last_brightness = brightness;
    brightness = 0;

    // compare current frame with last frame here
//    memset(histogram, 0, sizeof(histogram));
    memset(own_histogram, 0, sizeof(own_histogram));

//    max_delta =  min(videowidth,height)/2 - border;

    if (thread_count > 1) {
        static ScanWorkers workers({ScanBottom, ScanTop, ScanLeft, ScanRight});
        workers.run();
    } else {
        ScanBottom((intptr_t)0);
        ScanTop((intptr_t)0);
        ScanLeft((intptr_t)0);
        ScanRight((intptr_t)0);
    }
    for (i = 0; i < 256; i++) {
        histogram[i] = own_histogram[0][i] + own_histogram[1][i] + own_histogram[2][i] + own_histogram[3][i];
    }

#ifdef FRAME_WITH_HISTOGRAM
    if (framearray) memcpy(frame[frame_count].histogram, histogram, sizeof(histogram));
#endif
    if (framearray) frame[frame_count].minY = minY;
    if (framearray) frame[frame_count].maxY = maxY;
    if (framearray) frame[frame_count].minX = minX;
    if (framearray) frame[frame_count].maxX = maxX;

    if (framenum_real <= 1)
    {

        memcpy(lastHistogram, histogram, sizeof(histogram));
        last_brightness = brightness;


        if (commDetectMethod & AR)
        {
            ProcessARInfoInit(minY, maxY, minX, maxX);
            if (framearray) frame[frame_count].ar_ratio = last_ar_ratio;
        }

        ProcessACInfoInit(frame[frame_count].audio_channels);

        for (i = max_brightness; i >= 0; i--) last_brightness += histogram[i] * i;
        last_brightness /= (width - (border * 2)) * (height - (border * 2)) / 16;
        if (framearray)
        {
            frame[frame_count].brightness = last_brightness;
            frame[frame_count].logo_present = false;
            frame[frame_count].schange_percent = 0;
        }

        if (commDetectMethod & SCENE_CHANGE)
        {
            InitializeSchangeArray(0);
            schange[0].frame = 0;
            schange[0].percentage = 0;
            schange_count++;
        }

        return (false);
    }
    if ( 17652 < frame_count && frame_count < 17657 )
    {
//		OutputFrame(frame_count);
    }

    ProcessARInfo(minY, maxY,minX, maxX);
    ProcessACInfo(frame[frame_count].audio_channels);
    if (framearray) frame[frame_count].ar_ratio = last_ar_ratio;

    similar *= 1;

    for (i = 255; i > max_brightness; i--)
    {
        pixels += histogram[i];
        brightness += histogram[i] * i;
        if (histogram[i])
            hasBright++;
//		if (histogram[i] != lastHistogram[i]) similar += abs( histogram[i] - lastHistogram[i]);
        if (histogram[i] < lastHistogram[i]) similar += histogram[i];
        else similar += lastHistogram[i];
    }

    for (i = max_brightness; i > test_brightness; i--)
    {
        pixels += histogram[i];
        brightness += histogram[i] * i;
        dimCount += histogram[i];
//		if (histogram[i] != lastHistogram[i]) similar += abs( histogram[i] - lastHistogram[i]);
        if (histogram[i] < lastHistogram[i]) similar += histogram[i];
        else similar += lastHistogram[i];
    }

    for (i = test_brightness; i >= 0; i--)
    {
        pixels += histogram[i];
        brightness += histogram[i] * i;
//		if (histogram[i] != lastHistogram[i]) similar += abs( histogram[i] - lastHistogram[i]);
        if (histogram[i] < lastHistogram[i]) similar += histogram[i];
        else similar += lastHistogram[i];
    }
    brightness /= pixels;

    if (framearray) frame[frame_count].hasBright = hasBright;
    scale = 720.0 * 480.0 / width / height;
    if (min_hasBright > hasBright * scale)
        min_hasBright = hasBright * scale;

    if (framearray) frame[frame_count].dimCount = dimCount;
    if (min_dimCount > dimCount * scale)
        min_dimCount = dimCount * scale;

    if (cutsceneno != 0 || frame_count == cutsceneno)
        RecordCutScene(frame_count, brightness);



    uniform = 0;
    for (i = 255; i > brightness + noise_level; i--)
    {
        uniform +=  histogram[i] * (i - brightness);
    }
    for (i = brightness - noise_level; i >= 0; i--)
    {
        uniform +=  histogram[i] * (brightness - i);
    }
    uniform = ((double)uniform) * 730/pixels;
    if (framearray) frame[frame_count].uniform = uniform;


    x = 0;
    for (i=10; i<100; i++)
    {
        if (histogram[i] > 10)
        {
            if (x<histogram[i])
                x=histogram[i];
            else
            {
                if (x > histogram[i+1] && histogram[i-1] > 1000)
                {
                    blackHistogram[i-1]++;
                    break;
                }
            }
        }
    }
    x = 0;
    for (i=10; i<100; i++)
    {
        if (blackHistogram[x] < blackHistogram[i])
        {
            x = i;
        }
    }
    /*	Not tested
    	if (x > 10 && (frame_count % 2) == 0) {
    		x = x + 5;
    		if (x > max_avg_brightness) {
    			max_avg_brightness++;
    			test_brightness++;
    			max_brightness++;
    		} else if (x < max_avg_brightness) {
    			max_avg_brightness--;
    			test_brightness--;
    			max_brightness--;
    		}
    	}
    */
    if (framearray) frame[frame_count].brightness = brightness;
    brightHistogram[brightness]++;
    uniformHistogram[(uniform/UNIFORMSCALE < 255 ? uniform/UNIFORMSCALE : 255)]++;
    if ((dimCount > (int)(.05 * width * height)) && (dimCount < (int)(.35 * width * height))) isDim = true;

    sceneChangePercent = (int)(100.0 * similar / pixels);
//	sceneChangePercent = (int)(100.0 * (1.0 - ((float)abs(prevsimilar - similar) / pixels)));
//    prevsimilar = similar;

    if (framearray) frame[frame_count].schange_percent = sceneChangePercent;


//	cause = ProcessClues(frame_count, brightness, hasBright, isDim, uniform, sceneChangePercent, curvolume,
//	if (framearray) frame[frame_count].isblack = cause;
//	if (cause != 0)
//		InsertBlackFrame(framenum_real,brightness,uniform,curvolume,cause;

    cause = 0;
    if (commDetectMethod & BLACK_FRAME)
    {
        if ((brightness <= max_avg_brightness) && hasBright <= maxbright * width * height / 720 / 480 && !isDim /* && uniform < non_uniformity */  /* && !lastLogoTest because logo disappearance is detected too late*/)
        {
            cause |= C_b;
            Debug(7, "Frame %6i (%.3fs) - Black frame with brightness of %i,uniform of %i and volume of %i\n", framenum_real, get_frame_pts(framenum_real), brightness, uniform, black[MAX(0,black_count - 1)].volume);
        }
        else if (non_uniformity > 0)
        {
//????
            if ((brightness <= max_avg_brightness) && uniform < non_uniformity )
            {
                cause |= C_u;
                Debug(7, "Frame %6i (%.3fs) - Black frame with brightness of %i,uniform of %i and volume of %i\n", framenum_real, get_frame_pts(framenum_real), brightness, uniform, black[MAX(0,black_count - 1)].volume);
            }
            if (brightness > max_avg_brightness && uniform < non_uniformity && brightness < 250 )
            {
                cause |= C_u;
                Debug(7, "Frame %6i (%.3fs) - Uniform frame with brightness of %i and uniform of %i\n", framenum_real, get_frame_pts(framenum_real), brightness, uniform);
            }
        }
    }

 //   if (commDetectMethod & RESOLUTION_CHANGE)
    {
        if ((old_width != 0 && width != old_width) || (old_height != 0 && height != old_height))
        {
            cause |= C_r;
            Debug(7, "Frame %6i (%.3fs) - Resolution change from %d x %d to %d x %d \n", framenum_real, get_frame_pts(framenum_real), old_width, old_height, width, height);
            old_width = width;
            old_height = height;
            ResetLogoBuffers();
        }
        old_width = width;
        old_height = height;
    }


    /*
    	if (abs(brightness - last_brightness) > brightness_jump) {
    		cause |= C_s;
    		Debug(7, "Frame %6i - Black frame because large brightness change from %i to %i with uniform %i\n", framenum_real, last_brightness, brightness, uniform);
    	} // else
    */
    if (commDetectMethod & SCENE_CHANGE)
    {

        if (!(frame[frame_count-1].isblack & C_b) && !(cause & C_b))
        {
            if (abs(frame[frame_count-1].brightness - last_brightness) > brightness_jump)
            {
                Debug( 7,"Frame %6i (%.3fs) - Black frame because large brightness change from %i to %i with uniform %i\n", framenum_real, get_frame_pts(framenum_real), last_brightness, brightness, uniform);
                cause |= C_s;
            }
            else if (/* sceneChangePercent */ frame[frame_count-1].schange_percent < schange_cutlevel)
            {
                Debug( 7,"Frame %6i (%.3fs) - Black frame because large scene change of %i, uniform %i\n", framenum_real, get_frame_pts(framenum_real), sceneChangePercent, uniform);

                cause |= C_s;
            }
        }

        /*
        if ((sceneChangePercent < 10) && (!hasBright) && !(cause & C_b)) {
        Debug(
        7,
        "Frame %6i - BlackFrame detected because of a nonbright scene change:\tsc - %i\tavg - %i\n",
        framenum_real,
        sceneChangePercent,
        brightness
        );
        cause |= C_s;
        } else if ((sceneChangePercent < 20) && (!hasBright) && !(cause & C_b)) {




        		if (brightness < last_brightness * 2) {
        		InitializeSchangeArray(schange_count);
        		schange[schange_count].percentage = sceneChangePercent;
        		schange[schange_count].frame = framenum_real;
        		schange_count++;
        		memcpy(lastHistogram, histogram, sizeof(histogram));
        		//				Debug(7, "Frame %6i - Scene change with change percentage of %i\n", framenum_real, sceneChangePercent);
        		return (true);
        		}
        		if (0) {
        		Debug(
        		7,
        		"Frame %6i - BlackFrame detected because of scene change with brightness double:\tsc - %i\tavg - %i.................................................................\n",
        		framenum_real,
        		sceneChangePercent,
        		brightness
        		);
        		cause |= C_s;
        		}
        */

    }
    if (sceneChangePercent < schange_threshold)
    {
        // Scene Change threshold: original = 91
        InitializeSchangeArray(schange_count);
        schange[schange_count].percentage = sceneChangePercent;
        schange[schange_count].frame = framenum_real;
        schange_count++;
//	   memcpy(lastHistogram, histogram, sizeof(histogram));
        //			Debug(7, "Frame %6i (%.3fs) - Scene change with change percentage of %i\n", framenum_real, get_frame_pts(framenum_real), sceneChangePercent);
    }

//    for (i=0; i < 255; i++)               No used!!!!!!!!
//        if (histogram[i] > 10) break;

    if (brightness < min_brightness_found) min_brightness_found = brightness;

    if (framearray) frame[frame_count].cutscenematch = 100;
//	if (brightness > max_avg_brightness + 10)
    if (cutscenes)
    {

        if (framearray) frame[frame_count].cutscenematch = 100;
        for (i = 0; i < cutscenes; i++)
        {
            if (abs(brightness - csbrightness[i]) < 2)
            {
                cutscenematch = MatchCutScene(cutscene[i]);
                if (framearray)
                {
                    if (frame[frame_count].cutscenematch > cutscenematch*100/cslength[i])
                        frame[frame_count].cutscenematch = cutscenematch*100/cslength[i];
                    if (frame[frame_count].cutscenematch < cutscenedelta)
                        cause |= C_t;
                }
            }
        }

    } else {
 //       DetectCredits(frame_count);
    }
//    if (frame[frame_count].cutscenematch < cutscenedelta)
//        cause |= C_t;

    if (commDetectMethod & SILENCE)
    {
        if (0 <= frame[frame_count].volume && frame[frame_count].volume < max_silence && min_silence == 1)
        {
            cause |= C_v;
        }
        if (0 == frame[frame_count].volume)
        {
            cause |= C_v;
        }
    }

    if (cause != 0)
        InsertBlackFrame(framenum_real,brightness,uniform,curvolume,cause);
    if (framearray) frame[frame_count].isblack = cause;

    return (false);
}



// Subroutines for Logo Detection
