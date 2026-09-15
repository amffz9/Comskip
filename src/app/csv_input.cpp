#include "exit_requested.h"
#include "legacy_detection.h"

void PrintArgs(void)
{
    int i;
    for (i = 0; i < argument_count; i++) printf("%i\t%s\n", i, argument[i]);
}


#ifdef PROCESS_CC
extern "C" long process_block (unsigned char *data, long length);
#endif


void ProcessCSV(FILE *in_file)
{
    bool	lineProcessed = false;
    bool	lastLogoTest = false,curLogoTest = false;
//	bool	isDim = false;
    char	line[2048];
    char	split[256];
    int		cont = 0;

    int		minminY=10000,maxmaxY = 0;
    int		minminX=10000,maxmaxX = 0;
    int		cutscene_nonzero_count = 0;
    int old_format = true;
    int     use_bright = 0;
    double  t;
    int		i;
    int		x;
    int		f;
    int		col;
    int		ccDataFrame;

//	time_t	ltime;
again:
    logoInfoAvailable = true;
    if (!in_file)
    {
        Debug(0, "Something went wrong... Exiting...\n");
        comskip::request_exit(22);
    }
    fgets(line, sizeof(line), in_file); // Skip first line
    if (strcmp(line,"sep=,\n")==0)
        fgets(line, sizeof(line), in_file); // Skip second line
    t = 0.0;
    if (line[85] == ';') line [85] = '+';
    if (strlen(line) > 85)
    {

        t = ((double)strtol(&line[85], NULL, 10))/100;
        if (t > 99)
            t = t / 10.0;
    }
//   Debug(1, "T = %f\n",t);
    if (t > 0)
        fps = t  * 1.00000000000001;
    if (strlen(line) > 94)
    {
        t = ((double)strtol(&line[94], NULL, 10))/100;
        if (t > 99)
            t = t / 10.0;
    }
    if (t>0)
        fps = t  * 1.00000000000001;
    if (strlen(line) > 131)
    {
        t = strtod(&line[131], NULL);

        // Handle backward compatibility
        if (strchr(&line[131], '.') == NULL) {
            t /= 100.0;
            if (t > 99) {
                t /= 10.0;
            }

            if (t>0) {
                fps = t  * 1.00000000000001;
            }
        } else {
            if (t > 0) {
                fps = t;
            }
        }
    }
    InitComSkip();
    frame_count = 1;
    pict_type = '?';
    while (fgets(line, sizeof(line), in_file) != NULL)
    {
        i = 0;
        x = 0;
        col = 0;
        lineProcessed = false;
        InitializeFrameArray(frame_count);

//		i, frame[i].brightness, frame[i].schange_percent*5, frame[i].logo_present,
//				frame[i].uniform, frame[i].volume,  frame[i].minY,frame[i].maxY,(int)((frame[i].ar_ratio)*100),
//				(int)(frame[i].currentGoodEdge * 500), frame[i].isblack

        frame[frame_count].minX = 0;
        frame[frame_count].maxX = 0;
        frame[frame_count].hasBright = 0;
        frame[frame_count].dimCount = 0;
        frame[frame_count].pts = (frame_count - 1) / fps;
        frame[frame_count].pict_type = '?';
        frame[frame_count].audio_channels = 2;


        // Split Line Apart
        while (line[i] != '\0' && i < (int)sizeof(line) && !lineProcessed)
        {
            if (line[i] == ';' || line[i] == ',' || line[i] == '\n')
            {
                split[x] = '\0';

                // printf("col = %i\t", col);
                switch (col)
                {
                case 0:
                    f = strtol(split, NULL, 10);
                    if (f!= frame_count)
                    {
                        Debug(0, "Shit!!!!\n");
                        comskip::request_exit(23);
                    }
                    break;

                case 1:
                    frame[frame_count].brightness = strtol(split, NULL, 10);
                    break;

                case 2:
                    frame[frame_count].schange_percent = strtol(split, NULL, 10)/5;
                    break;

                case 3:
                    frame[frame_count].logo_present = strtol(split, NULL, 10);
                    break;

                case 4:
                    frame[frame_count].uniform = strtol(split, NULL, 10);
                    break;
                case 5:
                    frame[frame_count].volume = strtol(split, NULL, 10);
                    break;
                case 6:
                    frame[frame_count].minY = strtol(split, NULL, 10);
                    if (minminY > frame[frame_count].minY) minminY = frame[frame_count].minY;
                    break;
                case 7:
                    frame[frame_count].maxY = strtol(split, NULL, 10);
                    if (maxmaxY < frame[frame_count].maxY) maxmaxY = frame[frame_count].maxY;
                    break;
                case 8:
                    frame[frame_count].ar_ratio = strtod(split, NULL);
                    // Handle files that are before the values was written as a double
                    if (strchr(split, '.') == NULL) {
                        frame[frame_count].ar_ratio /= 100;
                    }
                    break;
                case 9:
                    frame[frame_count].currentGoodEdge = strtod(split, NULL);
                    // Handle files that are before the values was written as a double
                    if (strchr(split, '.') == NULL) {
                        frame[frame_count].currentGoodEdge /= 500;
                    }
                    break;
                case 10:
                    frame[frame_count].isblack = strtol(split, NULL, 10);
                    if (!(frame[frame_count].isblack == 0 || frame[frame_count].isblack == 1))
                        old_format = false;
                    break;
                case 11:
                    frame[frame_count].cutscenematch = strtol(split, NULL, 10);
                    if ( frame[frame_count].cutscenematch>0 ) cutscene_nonzero_count++;
                    break;
                case 12:
                    frame[frame_count].minX = strtol(split, NULL, 10);
                    if (minminX > frame[frame_count].minX) minminX = frame[frame_count].minX;
                    break;
                case 13:
                    frame[frame_count].maxX = strtol(split, NULL, 10);
                    if (maxmaxX < frame[frame_count].maxX) maxmaxX = frame[frame_count].maxX;
                    break;
                case 14:
                    frame[frame_count].hasBright = strtol(split, NULL, 10);
                    break;
                case 15:
                    frame[frame_count].dimCount = strtol(split, NULL, 10);
                    break;
                case 16:
                    frame[frame_count].pts = strtod(split, NULL);
                    break;
                case 17:
                    frame[frame_count].cur_segment = strtol(split, NULL, 10);
                    break;
                case 18:
                    frame[frame_count].audio_channels = strtol(split, NULL, 10);
                    break;


                default:
#ifdef FRAME_WITH_HISTOGRAM
                    frame[frame_count].histogram[col - 9] = strtol(split, NULL, 10);
#endif
                    break;
                }

                col++;
                x = 0;
                split[0] = '\0';
                if (col > 256 + 9) lineProcessed = true;
            }
            else
            {
                split[x] = line[i];
                x++;
            }

            i++;
        }
        if (frame_count == 1)
            frame[0].pts = frame[1].pts;
        frame_count++;
    }


    frame[frame_count].pts = (frame_count - 1) / fps; // Should be avg_fps, but is never used.

    if (!dump_data_file)
    {
        sprintf(line, "%s.data", workbasename);
        dump_data_file = myfopen(line, "rb");
    }
    ccDataFrame = 0;


    for (i=0; i < 1000 && i < frame_count; i++)
    {
        if (frame[i].hasBright > 0)
            use_bright = 1;
    }


    height = maxmaxY + minminY;
    videowidth = width = maxmaxX + minminX;

    last_brightness = frame[1].brightness;
    Debug(8, "CSV file loaded into memory.\n");
    fclose(in_file);
    in_file = NULL;
    black_count = 0;
    logo_block_count = 0;
    black_count = 0;
    schange_count = 0;
    min_brightness_found = 255;
    for (i = 1; i < frame_count; i++)
    {
        framenum_real = i;
ccagain:
        if (dump_data_file && ccDataFrame == 0)
        {
            cont = fread(line,8,1,dump_data_file);
            line[8]=0;
            sscanf(line,"%7d:",&ccDataFrame);
//			ccDataFrame = strtol(line,NULL,7);
        }
        if (dump_data_file )
        {

            while (cont && ccDataFrame <=i)
            {

                cont = fread(line,4,1,dump_data_file);
                if (!cont)
                    break;
                line[4]=0;
                sscanf(line,"%4d",&ccDataLen);
//			ccDataLen = strtol(line,NULL,4);
                cont = fread(ccData,ccDataLen,1, dump_data_file);
                if (!cont)
                    break;
                framenum = ccDataFrame;
#ifdef PROCESS_CC
                if (processCC) ProcessCCData();
                if (output_srt || output_smi) process_block(ccData, (int)ccDataLen);
#endif
                ccDataFrame = 0;
                goto ccagain;
            }

        }
        if (old_format)
        {
            if (frame[i].isblack)
            {
                if (frame[i].brightness <= 5)
                {
                    if (frame[i].brightness == 5)
                        frame[i].isblack = C_a;
                    if (frame[i].brightness == 4)
                        frame[i].isblack = C_u;         // Checked
                    if (frame[i].brightness == 3)
                        frame[i].isblack = C_s;
                    if (frame[i].brightness == 2)
                        frame[i].isblack = C_s;			// Checked
                    if (frame[i].brightness == 1)
                        frame[i].isblack = C_u;
                    frame[i].brightness = max_avg_brightness + 1;
                }
                else
                    frame[i].isblack = C_b;
            }
            else
                frame[i].isblack = 0;
        }
        else
        {
//			frame[i].isblack &= C_b;
        }

        if (frame[i].brightness > 0)
        {
            brightHistogram[frame[i].brightness]++;

            if (frame[i].brightness < min_brightness_found) min_brightness_found = frame[i].brightness;

            uniformHistogram[(frame[i].uniform / UNIFORMSCALE < 255 ? frame[i].uniform / UNIFORMSCALE : 255)]++;
        }

        if (frame[i].volume >= 0)
        {
            volumeHistogram[(frame[i].volume/volumeScale < 255 ? frame[i].volume/volumeScale : 255)]++;
            silenceHistogram[(frame[i].volume < 255 ? frame[i].volume : 255)]++;
        }


        if (frame[i].maxX == 0)
        {
            if (i == 1)
            {
                if (frame[i].ar_ratio < 0.5)
                {
                    if (frame[i].maxY + frame[i].minY < 600)
                        videowidth = width = 720;
                    else if (frame[i].maxY + frame[i].minY < 800)
                        videowidth = width = 1200;
                    else
                        videowidth = width = 1920;
                }
                else
                    videowidth = width = (int) ((frame[i].maxY + frame[i].minY) * frame[i].ar_ratio );
            }
            frame[i].maxX = videowidth - 10;
            frame[i].minX = 10;
        }
        if (i == 1)
        {
//			if (frame[i].maxX == 0)
//				videowidth = width = (int) ((frame[i].maxY - frame[i].minY) * frame[i].ar_ratio );
            ProcessARInfoInit(frame[i].minY, frame[i].maxY, frame[i].minX, frame[i].maxX);
            ProcessACInfoInit(frame[i].audio_channels);
        }
        else
        {
            ProcessARInfo(frame[i].minY, frame[i].maxY, frame[i].minX, frame[i].maxX);
            ProcessACInfo(frame[i].audio_channels);
        }
        frame[i].ar_ratio = last_ar_ratio;


        if ((commDetectMethod & RESOLUTION_CHANGE))
        {
            /* not reliable!!!!!!!!!!!!!!!!!!!!!
            			frame[i].isblack &= ~C_r;
            			videowidth = width = frame[i].minX + frame[i].maxX;
            			height = frame[i].minY + frame[i].maxY;

            			if ((old_width != 0 && abs(width-old_width) > 50) || (old_height != 0 && abs(height - old_height) > 50)) {
            				frame[i].isblack |= C_r;
            			}
            			old_width = width;
            			old_height = height;
            */
        }
        else
            frame[i].isblack &= ~C_r;

        if (commDetectMethod & BLACK_FRAME)
        {
            // if (frame[i].brightness <= max_avg_brightness && (non_uniformity == 0 || frame[i].uniform < non_uniformity)/* && frame[i].volume < max_volume */ && !(frame[i].isblack & C_b))
            //    frame[i].isblack |= C_b;
            if ((frame[i].isblack & C_b) && frame[i].brightness > max_avg_brightness)
                frame[i].isblack &= ~C_b;

            if (use_bright)
            {

                if (frame[i].hasBright > 0 && min_hasBright > frame[i].hasBright * 720 * 480 / videowidth / height) min_hasBright = frame[i].hasBright * 720 * 480 / videowidth / height;
                if (frame[i].dimCount > 0 && min_dimCount > frame[i].dimCount * 720 * 480 / videowidth / height) min_dimCount = frame[i].dimCount * 720 * 480 / videowidth / height;

                if (frame[i].brightness <= max_avg_brightness && frame[i].hasBright < maxbright && frame[i].dimCount < (int)(.05 * videowidth * height))
                    frame[i].isblack |= C_b;
            }
            if (i>1) { // Uniform not calculated for frame 1
                frame[i].isblack &= ~C_u;
                if (!(frame[i].isblack & C_b) && non_uniformity > 0 && frame[i].uniform < non_uniformity && frame[i].brightness < 250 /*&& frame[i].volume < max_volume*/ )
                    frame[i].isblack |= C_u;
            }
        }
        else
        {
            frame[i].isblack &= ~(C_u | C_b);
        }

        if (frame[i].isblack & C_s)
            frame[i].isblack &= ~C_s;


        if (commDetectMethod & SCENE_CHANGE && !(frame[i-1].isblack & C_b) && !(frame[i].isblack & C_b))
        {
            if (frame[i].brightness > 5 && abs(frame[i].brightness - last_brightness) > brightness_jump)
            {
                frame[i].isblack |= C_s;
            }
            if (frame[i].brightness > 5)
                last_brightness = frame[i].brightness;

            if (frame[i].brightness > 5 && frame[i].schange_percent < 15)
            {
                frame[i].isblack |= C_s;
            }
        }


        if (frame[i].isblack & C_t)
            frame[i].isblack &= ~C_t;

        if (commDetectMethod & CUTSCENE && cutscene_nonzero_count > 0)
        {
            if (frame[i].cutscenematch < cutscenedelta)
                frame[i].isblack |= C_t;
        }

        if (frame[i].isblack & C_v)
            frame[i].isblack &= ~C_v;

        if (commDetectMethod & SILENCE)
        {
            if (0 <= frame[i].volume && frame[i].volume < max_silence && min_silence == 1)
            {
                frame[i].isblack |= C_v;
            }
            if (frame[i].volume < 6)
            {
                frame[i].isblack |= C_v;
            }
        }


        if (frame[i].isblack)
        {
            InsertBlackFrame(i,frame[i].brightness,frame[i].uniform,frame[i].volume, (int)frame[i].isblack);

            /*
            			j = i-volume_slip;
            			if (j < 0) j = 0;
            			k = i+volume_slip;
            			if (k>frame_count) k = frame_count;
            			for (x=j; x<k; x++)
            				if (frame[x].volume >= 0)
            					if (black[black_count].volume > frame[x].volume)
            						black[black_count].volume = frame[x].volume;
            //			if (black[black_count].volume < max_volume) frame[i].volume = 1;
            */
        }

        if ((frame[i].schange_percent < 20) && i > 1 && black_count > 0 && (black[black_count - 1].frame != i))
        {
            if (frame[i].brightness < frame[i - 1].brightness * 2)
            {
                InitializeSchangeArray(schange_count);
                schange[schange_count].percentage = frame[i].schange_percent;
                schange[schange_count].frame = i;
                schange_count++;
            }
        }
        else if (frame[i].schange_percent < schange_threshold)
        {

            // Scene Change threshold: original = 91
            InitializeSchangeArray(schange_count);
            schange[schange_count].percentage = frame[i].schange_percent;
            schange[schange_count].frame = i;
            schange_count++;
        }

        if ((commDetectMethod & LOGO) && ((i % (int)(fps * logoFreq)) == 0))
        {
            curLogoTest = (frame[i].currentGoodEdge > logo_threshold);
            lastLogoTest = ProcessLogoTest(i, curLogoTest, false);
            frame[i].logo_present = lastLogoTest;
        }
        if (lastLogoTest) frames_with_logo++;

//		if (live_tv && !frame[i].isblack) {
//			BuildCommListAsYouGo();
//		}
//        DetectCredits(i);

    }
    framenum_real = frame_count;
    framesprocessed = frame_count;

    if (output_live) {
        OutputBlackArray();
        BuildCommListAsYouGo();
    }

    BuildMasterCommList();

    if (output_debugwindow)
    {
#ifdef DEBUG
        skip_B_frames=0;
#endif
#ifdef DONATOR
        skip_B_frames=0;
#endif
        processCC = 0;
        i = 0;
        printf("Close window when done\n");
        if (ReviewResult())
        {
            LoadIniFile();
            goto again;
        }
        //		printf(" Press Enter to close debug window\n");
//		gets(HomeDir);
    }
    comskip::request_exit(0);
}

