#include "exit_requested.h"
#include "legacy_detection.h"

void PrintArgs(RecordingContext& context)
{
    for (std::size_t i = 0; i < context.state.argument.size(); ++i)
        printf("%zu\t%s\n", i, context.state.argument[i].c_str());
}


#ifdef PROCESS_CC
extern "C" long process_block (unsigned char *data, long length);
#endif


void ProcessCSV(RecordingContext& context, comskip::platform::FilePtr input)
{
    FILE* in_file = input.get();
    bool	lineProcessed = false;
    bool	lastLogoTest = false,curLogoTest = false;
//	bool	isDim = false;
    char	line[2048]{};
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
    context.state.logoInfoAvailable = true;
    if (!in_file)
    {
        Debug(context, 0, "Something went wrong... Exiting...\n");
        comskip::request_exit(22);
    }
    if (!fgets(line, sizeof(line), in_file))
        throw std::invalid_argument("CSV input has no header");
    if (strcmp(line,"sep=,\n")==0)
        if (!fgets(line, sizeof(line), in_file))
            throw std::invalid_argument("CSV input has no column header");
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
        context.settings.fps = t  * 1.00000000000001;
    if (strlen(line) > 94)
    {
        t = ((double)strtol(&line[94], NULL, 10))/100;
        if (t > 99)
            t = t / 10.0;
    }
    if (t>0)
        context.settings.fps = t  * 1.00000000000001;
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
                context.settings.fps = t  * 1.00000000000001;
            }
        } else {
            if (t > 0) {
                context.settings.fps = t;
            }
        }
    }
    InitComSkip(context);
    context.state.frame_count = 1;
    context.state.pict_type = '?';
    while (fgets(line, sizeof(line), in_file) != NULL)
    {
        i = 0;
        x = 0;
        col = 0;
        lineProcessed = false;
        InitializeFrameArray(context, context.state.frame_count);

//		i, frame[i].brightness, frame[i].schange_percent*5, frame[i].logo_present,
//				frame[i].uniform, frame[i].volume,  frame[i].minY,frame[i].maxY,(int)((frame[i].ar_ratio)*100),
//				(int)(frame[i].currentGoodEdge * 500), frame[i].isblack

        context.state.frame[context.state.frame_count].minX = 0;
        context.state.frame[context.state.frame_count].maxX = 0;
        context.state.frame[context.state.frame_count].hasBright = 0;
        context.state.frame[context.state.frame_count].dimCount = 0;
        context.state.frame[context.state.frame_count].pts = (context.state.frame_count - 1) / context.settings.fps;
        context.state.frame[context.state.frame_count].pict_type = '?';
        context.state.frame[context.state.frame_count].audio_channels = 2;


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
                    if (f!= context.state.frame_count)
                    {
                        Debug(context, 0, "Shit!!!!\n");
                        comskip::request_exit(23);
                    }
                    break;

                case 1:
                    context.state.frame[context.state.frame_count].brightness = strtol(split, NULL, 10);
                    break;

                case 2:
                    context.state.frame[context.state.frame_count].schange_percent = strtol(split, NULL, 10)/5;
                    break;

                case 3:
                    context.state.frame[context.state.frame_count].logo_present = strtol(split, NULL, 10);
                    break;

                case 4:
                    context.state.frame[context.state.frame_count].uniform = strtol(split, NULL, 10);
                    break;
                case 5:
                    context.state.frame[context.state.frame_count].volume = strtol(split, NULL, 10);
                    break;
                case 6:
                    context.state.frame[context.state.frame_count].minY = strtol(split, NULL, 10);
                    if (minminY > context.state.frame[context.state.frame_count].minY) minminY = context.state.frame[context.state.frame_count].minY;
                    break;
                case 7:
                    context.state.frame[context.state.frame_count].maxY = strtol(split, NULL, 10);
                    if (maxmaxY < context.state.frame[context.state.frame_count].maxY) maxmaxY = context.state.frame[context.state.frame_count].maxY;
                    break;
                case 8:
                    context.state.frame[context.state.frame_count].ar_ratio = strtod(split, NULL);
                    // Handle files that are before the values was written as a double
                    if (strchr(split, '.') == NULL) {
                        context.state.frame[context.state.frame_count].ar_ratio /= 100;
                    }
                    break;
                case 9:
                    context.state.frame[context.state.frame_count].currentGoodEdge = strtod(split, NULL);
                    // Handle files that are before the values was written as a double
                    if (strchr(split, '.') == NULL) {
                        context.state.frame[context.state.frame_count].currentGoodEdge /= 500;
                    }
                    break;
                case 10:
                    context.state.frame[context.state.frame_count].isblack = strtol(split, NULL, 10);
                    if (!(context.state.frame[context.state.frame_count].isblack == 0 || context.state.frame[context.state.frame_count].isblack == 1))
                        old_format = false;
                    break;
                case 11:
                    context.state.frame[context.state.frame_count].cutscenematch = strtol(split, NULL, 10);
                    if ( context.state.frame[context.state.frame_count].cutscenematch>0 ) cutscene_nonzero_count++;
                    break;
                case 12:
                    context.state.frame[context.state.frame_count].minX = strtol(split, NULL, 10);
                    if (minminX > context.state.frame[context.state.frame_count].minX) minminX = context.state.frame[context.state.frame_count].minX;
                    break;
                case 13:
                    context.state.frame[context.state.frame_count].maxX = strtol(split, NULL, 10);
                    if (maxmaxX < context.state.frame[context.state.frame_count].maxX) maxmaxX = context.state.frame[context.state.frame_count].maxX;
                    break;
                case 14:
                    context.state.frame[context.state.frame_count].hasBright = strtol(split, NULL, 10);
                    break;
                case 15:
                    context.state.frame[context.state.frame_count].dimCount = strtol(split, NULL, 10);
                    break;
                case 16:
                    context.state.frame[context.state.frame_count].pts = strtod(split, NULL);
                    break;
                case 17:
                    context.state.frame[context.state.frame_count].cur_segment = strtol(split, NULL, 10);
                    break;
                case 18:
                    context.state.frame[context.state.frame_count].audio_channels = strtol(split, NULL, 10);
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
        if (context.state.frame_count == 1)
            context.state.frame[0].pts = context.state.frame[1].pts;
        context.state.frame_count++;
    }


    context.state.frame[context.state.frame_count].pts = (context.state.frame_count - 1) / context.settings.fps; // Should be avg_fps, but is never used.

    if (!context.state.dump_data_file.get())
    {
        sprintf(line, "%s.data", context.state.workbasename);
        context.state.dump_data_file.reset(myfopen(line, "rb"));
    }
    ccDataFrame = 0;


    for (i=0; i < 1000 && i < context.state.frame_count; i++)
    {
        if (context.state.frame[i].hasBright > 0)
            use_bright = 1;
    }


    context.state.height = maxmaxY + minminY;
    context.state.videowidth = context.state.width = maxmaxX + minminX;

    context.state.last_brightness = context.state.frame[1].brightness;
    Debug(context, 8, "CSV file loaded into memory.\n");
    input.reset();
    in_file = NULL;
    context.state.black_count = 0;
    context.state.logo_block_count = 0;
    context.state.black_count = 0;
    context.state.schange_count = 0;
    context.state.min_brightness_found = 255;
    for (i = 1; i < context.state.frame_count; i++)
    {
        context.state.framenum_real = i;
ccagain:
        if (context.state.dump_data_file.get() && ccDataFrame == 0)
        {
            cont = fread(line,8,1,context.state.dump_data_file.get());
            line[8]=0;
            sscanf(line,"%7d:",&ccDataFrame);
//			ccDataFrame = strtol(line,NULL,7);
        }
        if (context.state.dump_data_file.get() )
        {

            while (cont && ccDataFrame <=i)
            {

                cont = fread(line,4,1,context.state.dump_data_file.get());
                if (!cont)
                    break;
                line[4]=0;
                sscanf(line,"%4d",&context.state.ccDataLen);
//			ccDataLen = strtol(line,NULL,4);
                cont = fread(context.state.ccData,context.state.ccDataLen,1, context.state.dump_data_file.get());
                if (!cont)
                    break;
                context.state.framenum = ccDataFrame;
#ifdef PROCESS_CC
                if (context.state.processCC) ProcessCCData(context);
                if (context.settings.output_srt || context.settings.output_smi) process_block(context.state.ccData, (int)context.state.ccDataLen);
#endif
                ccDataFrame = 0;
                goto ccagain;
            }

        }
        if (old_format)
        {
            if (context.state.frame[i].isblack)
            {
                if (context.state.frame[i].brightness <= 5)
                {
                    if (context.state.frame[i].brightness == 5)
                        context.state.frame[i].isblack = C_a;
                    if (context.state.frame[i].brightness == 4)
                        context.state.frame[i].isblack = C_u;         // Checked
                    if (context.state.frame[i].brightness == 3)
                        context.state.frame[i].isblack = C_s;
                    if (context.state.frame[i].brightness == 2)
                        context.state.frame[i].isblack = C_s;			// Checked
                    if (context.state.frame[i].brightness == 1)
                        context.state.frame[i].isblack = C_u;
                    context.state.frame[i].brightness = context.settings.max_avg_brightness + 1;
                }
                else
                    context.state.frame[i].isblack = C_b;
            }
            else
                context.state.frame[i].isblack = 0;
        }
        else
        {
//			frame[i].isblack &= C_b;
        }

        if (context.state.frame[i].brightness > 0)
        {
            context.state.brightHistogram[std::clamp(context.state.frame[i].brightness, 0, 255)]++;

            if (context.state.frame[i].brightness < context.state.min_brightness_found) context.state.min_brightness_found = context.state.frame[i].brightness;

            context.state.uniformHistogram[std::clamp(context.state.frame[i].uniform / UNIFORMSCALE, 0, 255)]++;
        }

        if (context.state.frame[i].volume >= 0)
        {
            context.state.volumeHistogram[(context.state.frame[i].volume/context.state.volumeScale < 255 ? context.state.frame[i].volume/context.state.volumeScale : 255)]++;
            context.state.silenceHistogram[(context.state.frame[i].volume < 255 ? context.state.frame[i].volume : 255)]++;
        }


        if (context.state.frame[i].maxX == 0)
        {
            if (i == 1)
            {
                if (context.state.frame[i].ar_ratio < 0.5)
                {
                    if (context.state.frame[i].maxY + context.state.frame[i].minY < 600)
                        context.state.videowidth = context.state.width = 720;
                    else if (context.state.frame[i].maxY + context.state.frame[i].minY < 800)
                        context.state.videowidth = context.state.width = 1200;
                    else
                        context.state.videowidth = context.state.width = 1920;
                }
                else
                    context.state.videowidth = context.state.width = (int) ((context.state.frame[i].maxY + context.state.frame[i].minY) * context.state.frame[i].ar_ratio );
            }
            context.state.frame[i].maxX = context.state.videowidth - 10;
            context.state.frame[i].minX = 10;
        }
        if (i == 1)
        {
//			if (frame[i].maxX == 0)
//				videowidth = width = (int) ((frame[i].maxY - frame[i].minY) * frame[i].ar_ratio );
            ProcessARInfoInit(context, context.state.frame[i].minY, context.state.frame[i].maxY, context.state.frame[i].minX, context.state.frame[i].maxX);
            ProcessACInfoInit(context, context.state.frame[i].audio_channels);
        }
        else
        {
            ProcessARInfo(context, context.state.frame[i].minY, context.state.frame[i].maxY, context.state.frame[i].minX, context.state.frame[i].maxX);
            ProcessACInfo(context, context.state.frame[i].audio_channels);
        }
        context.state.frame[i].ar_ratio = context.state.last_ar_ratio;


        if ((context.settings.commDetectMethod & RESOLUTION_CHANGE))
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
            context.state.frame[i].isblack &= ~C_r;

        if (context.settings.commDetectMethod & BLACK_FRAME)
        {
            // if (frame[i].brightness <= max_avg_brightness && (non_uniformity == 0 || frame[i].uniform < non_uniformity)/* && frame[i].volume < max_volume */ && !(frame[i].isblack & C_b))
            //    frame[i].isblack |= C_b;
            if ((context.state.frame[i].isblack & C_b) && context.state.frame[i].brightness > context.settings.max_avg_brightness)
                context.state.frame[i].isblack &= ~C_b;

            if (use_bright)
            {

                if (context.state.frame[i].hasBright > 0 && context.state.min_hasBright > context.state.frame[i].hasBright * 720 * 480 / context.state.videowidth / context.state.height) context.state.min_hasBright = context.state.frame[i].hasBright * 720 * 480 / context.state.videowidth / context.state.height;
                if (context.state.frame[i].dimCount > 0 && context.state.min_dimCount > context.state.frame[i].dimCount * 720 * 480 / context.state.videowidth / context.state.height) context.state.min_dimCount = context.state.frame[i].dimCount * 720 * 480 / context.state.videowidth / context.state.height;

                if (context.state.frame[i].brightness <= context.settings.max_avg_brightness && context.state.frame[i].hasBright < context.settings.maxbright && context.state.frame[i].dimCount < (int)(.05 * context.state.videowidth * context.state.height))
                    context.state.frame[i].isblack |= C_b;
            }
            if (i>1) { // Uniform not calculated for frame 1
                context.state.frame[i].isblack &= ~C_u;
                if (!(context.state.frame[i].isblack & C_b) && context.settings.non_uniformity > 0 && context.state.frame[i].uniform < context.settings.non_uniformity && context.state.frame[i].brightness < 250 /*&& frame[i].volume < max_volume*/ )
                    context.state.frame[i].isblack |= C_u;
            }
        }
        else
        {
            context.state.frame[i].isblack &= ~(C_u | C_b);
        }

        if (context.state.frame[i].isblack & C_s)
            context.state.frame[i].isblack &= ~C_s;


        if (context.settings.commDetectMethod & SCENE_CHANGE && !(context.state.frame[i-1].isblack & C_b) && !(context.state.frame[i].isblack & C_b))
        {
            if (context.state.frame[i].brightness > 5 && abs(context.state.frame[i].brightness - context.state.last_brightness) > context.settings.brightness_jump)
            {
                context.state.frame[i].isblack |= C_s;
            }
            if (context.state.frame[i].brightness > 5)
                context.state.last_brightness = context.state.frame[i].brightness;

            if (context.state.frame[i].brightness > 5 && context.state.frame[i].schange_percent < 15)
            {
                context.state.frame[i].isblack |= C_s;
            }
        }


        if (context.state.frame[i].isblack & C_t)
            context.state.frame[i].isblack &= ~C_t;

        if (context.settings.commDetectMethod & CUTSCENE && cutscene_nonzero_count > 0)
        {
            if (context.state.frame[i].cutscenematch < context.settings.cutscenedelta)
                context.state.frame[i].isblack |= C_t;
        }

        if (context.state.frame[i].isblack & C_v)
            context.state.frame[i].isblack &= ~C_v;

        if (context.settings.commDetectMethod & SILENCE)
        {
            if (0 <= context.state.frame[i].volume && context.state.frame[i].volume < context.settings.max_silence && context.settings.min_silence == 1)
            {
                context.state.frame[i].isblack |= C_v;
            }
            if (context.state.frame[i].volume < 6)
            {
                context.state.frame[i].isblack |= C_v;
            }
        }


        if (context.state.frame[i].isblack)
        {
            InsertBlackFrame(context, i,context.state.frame[i].brightness,context.state.frame[i].uniform,context.state.frame[i].volume, (int)context.state.frame[i].isblack);

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

        if ((context.state.frame[i].schange_percent < 20) && i > 1 && context.state.black_count > 0 && (context.state.black[context.state.black_count - 1].frame != i))
        {
            if (context.state.frame[i].brightness < context.state.frame[i - 1].brightness * 2)
            {
                InitializeSchangeArray(context, context.state.schange_count);
                context.state.schange[context.state.schange_count].percentage = context.state.frame[i].schange_percent;
                context.state.schange[context.state.schange_count].frame = i;
                context.state.schange_count++;
            }
        }
        else if (context.state.frame[i].schange_percent < context.state.schange_threshold)
        {

            // Scene Change threshold: original = 91
            InitializeSchangeArray(context, context.state.schange_count);
            context.state.schange[context.state.schange_count].percentage = context.state.frame[i].schange_percent;
            context.state.schange[context.state.schange_count].frame = i;
            context.state.schange_count++;
        }

        if ((context.settings.commDetectMethod & LOGO) && ((i % (int)(context.settings.fps * context.state.logoFreq)) == 0))
        {
            curLogoTest = (context.state.frame[i].currentGoodEdge > context.settings.logo_threshold);
            lastLogoTest = ProcessLogoTest(context, i, curLogoTest, false);
            context.state.frame[i].logo_present = lastLogoTest;
        }
        if (lastLogoTest) context.state.frames_with_logo++;

//		if (live_tv && !frame[i].isblack) {
//			BuildCommListAsYouGo();
//		}
//        DetectCredits(i);

    }
    context.state.framenum_real = context.state.frame_count;
    context.state.framesprocessed = context.state.frame_count;

    if (context.settings.output_live) {
        OutputBlackArray(context);
        BuildCommListAsYouGo(context);
    }

    BuildMasterCommList(context);

    if (context.settings.output_debugwindow)
    {
#ifdef DEBUG
        skip_B_frames=0;
#endif
#ifdef DONATOR
        context.settings.skip_B_frames=0;
#endif
        context.state.processCC = 0;
        i = 0;
        printf("Close window when done\n");
        if (ReviewResult(context))
        {
            LoadIniFile(context);
            goto again;
        }
        //		printf(" Press Enter to close debug window\n");
//		gets(HomeDir);
    }
    comskip::request_exit(0);
}

