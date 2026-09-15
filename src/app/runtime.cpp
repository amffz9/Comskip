#include "exit_requested.h"
#include "legacy_detection.h"

int CountSceneChanges(int StartFrame, int EndFrame)
{
    int i;
    double p = 0;
    int count = 0;
    for (i = 0; i < schange_count; i++)
    {
        if ((schange[i].frame > StartFrame) && (schange[i].frame < EndFrame))
        {
            count++;
            p += (double)(100 - schange[i].percentage)  / (100 - schange_threshold);
        }
    }
    count = (int) p;

    return (count);
}

void Debug(int level, const char * fmt, ...)
{
    va_list	ap;
    FILE *log_file = NULL;
    if(verbose < level) return;

    va_start(ap, fmt);
    vsnprintf(debugText, sizeof(debugText), fmt, ap);
    va_end(ap);

    if (output_console)	_cprintf("%s", debugText);

    if (!log_file)
        log_file = myfopen(logfilename, "a+");

    if (log_file)
    {
        fprintf(log_file, "%s", debugText);
        fclose(log_file);
        log_file = NULL;
    }


    debugText[0] = '\0';
}

void InitLogoBuffers(void)
{
    int i;
    if(!logoFrameNum) logoFrameNum = static_cast<int *>( malloc(num_logo_buffers * sizeof(int)) );
    if (logoFrameNum == NULL)
    {
        Debug(0, "Could not allocate memory for logo buffer frame number array\n");
        comskip::request_exit(14);
    }
    memset(logoFrameNum, 0,num_logo_buffers*sizeof(int));
    /*
    	if(!choriz_edgemask) choriz_edgemask = malloc(width * height * sizeof(unsigned char));
    	if (choriz_edgemask == NULL) {
    		Debug(0, "Could not allocate memory for horizontal edgemask\n");
            comskip::request_exit(14);
    	}

    	if(!cvert_edgemask) cvert_edgemask = malloc(width * height * sizeof(unsigned char));
    	if (cvert_edgemask == NULL) {
    		Debug(0, "Could not allocate memory for vertical edgemask\n");
            comskip::request_exit(15);
    	}
    */
    if(!logoFrameBuffer)
    {
        logoFrameBuffer = static_cast<unsigned char **>( malloc(num_logo_buffers * sizeof(unsigned char *)) );
        if (!(logoFrameBuffer == NULL))
        {

            lheight = MAXHEIGHT;
            lwidth = MAXWIDTH;
            logoFrameBufferSize = lwidth * lheight * sizeof(frame_ptr[0]);
            for (i = 0; i < num_logo_buffers; i++)
            {
                logoFrameBuffer[i] = static_cast<unsigned char *>( malloc(logoFrameBufferSize) );
                if (logoFrameBuffer[i] == NULL)
                {
                    Debug(0, "Could not allocate memory for logo frame buffer %i\n", i);
                    comskip::request_exit(16);
                }
            }
        }
        else
        {
            Debug(0, "Could not allocate memory for logo frame buffers\n");
            comskip::request_exit(16);
        }
    }
#if MULTI_EDGE_BUFFER
    if(!horiz_edges)
    {

        horiz_edges = malloc(num_logo_buffers * sizeof(unsigned char *));
        if (!(horiz_edges == NULL))
        {
            for (i = 0; i < num_logo_buffers; i++)
            {
                horiz_edges[i] = malloc(width * height * sizeof(unsigned char));
                if (horiz_edges[i] == NULL)
                {
                    Debug(0, "Could not allocate memory for horizontal edge buffer %i\n", i);
                    comskip::request_exit(17);
                }
            }
        }
        else
        {
            Debug(0, "Could not allocate memory for horizontal edge buffers\n");
            comskip::request_exit(18);
        }
    }
#else
    /*
    	horiz_count = malloc(width * height * sizeof(unsigned char));
    	if (horiz_count == NULL) {
    		Debug(0, "Could not allocate memory for horizontal count buffer\n");
            comskip::request_exit(17);
    	}
    	memset(horiz_count, 0, width * height * sizeof(unsigned char));
    */
#endif

#if MULTI_EDGE_BUFFER
    if(!vert_edges)
    {
        vert_edges = malloc(num_logo_buffers * sizeof(unsigned char *));
        if (!(vert_edges == NULL))
        {
            for (i = 0; i < num_logo_buffers; i++)
            {
                vert_edges[i] = malloc(width * height * sizeof(unsigned char));
                if (vert_edges[i] == NULL)
                {
                    Debug(0, "Could not allocate memory for vertical edge buffer %i\n", i);
                    comskip::request_exit(19);
                }
            }
        }
        else
        {
            Debug(0, "Could not allocate memory for vertical edge buffers\n");
            comskip::request_exit(20);
        }
    }
#else
    /*
    	vert_count = malloc(width * height * sizeof(unsigned char));
    	if (vert_count == NULL) {
    		Debug(0, "Could not allocate memory for vertical count buffer\n");
            comskip::request_exit(17);
    	}
    	memset(vert_count, 0, width * height * sizeof(unsigned char));
    */
#endif
}

void Init_XDS_block();

void InitComSkip(void)
{
    int i, j;
    min_brightness_found = 255;
    max_logo_gap = -1;
    max_nonlogo_block_length = -1;
    logo_overshoot = 0.0;
    for (i = 0; i < 256; i++) brightHistogram[i] = 0;
    for (i = 0; i < 256; i++) uniformHistogram[i] = 0;
    for (i = 0; i < 256; i++) volumeHistogram[i] = 0;
    for (i = 0; i < 256; i++) silenceHistogram[i] = 0;

    if (framearray)
    {
        if(!initialized)
        {
            max_frame_count = (int)(60 * 60 * fps) + 1;
            frame = static_cast<frame_info *>( malloc((int)((max_frame_count + 1) * sizeof(frame_info))) );
        }
        if (frame == NULL)
        {
            Debug(0, "Could not allocate memory for frame array\n");
            comskip::request_exit(10);
        }
    }

//	if (commDetectMethod & BLACK_FRAME) {
    if(!initialized)
    {
        max_black_count = 500;
        black = static_cast<black_frame_info *>( malloc((int)((max_black_count + 1) * sizeof(black_frame_info))) );
    }
    if (black == NULL)
    {
        Debug(0, "Could not allocate memory for black frame array\n");
        comskip::request_exit(11);
    }
//	} else {
//		Debug(1, "ERROR: ComSkip cannot run without black frames.\n");
//		comskip::request_exit(100);
//	}

    if (commDetectMethod & LOGO)
    {
        if(!initialized)
        {
            max_logo_block_count = 1000;
            logo_block = static_cast<logo_block_info *>( malloc((int)((max_logo_block_count + 1) * sizeof(logo_block_info))) );
        }
        if (logo_block == NULL)
        {
            Debug(0, "Could not allocate memory for logo cblock array\n");
            comskip::request_exit(13);
        }

//		if (!logoInfoAvailable) {
        InitLogoBuffers();
//		}
        memset(max_br,   0, sizeof(max_br));
        memset(min_br, 255, sizeof(min_br));
    }

    if (commDetectMethod & SCENE_CHANGE)
    {
        if(!initialized)
        {
            max_schange_count = 2000;
            schange = static_cast<schange_info *>( malloc((int)((max_schange_count + 1) * sizeof(schange_info))) );
        }
        if (schange == NULL)
        {
            Debug(0, "Could not allocate memory for scene change array\n");
            comskip::request_exit(12);
        }
    }

    if (processCC)
    {
        if(!initialized)
        {
            max_cc_block_count = 500;
            cc_block = static_cast<cc_block_info *>( malloc((max_cc_block_count + 1) * sizeof(cc_block_info)) );
        }
        if (cc_block == NULL)
        {
            Debug(0, "Could not allocate memory for cc blocks\n");
            comskip::request_exit(22);
        }

        cc_block[0].start_frame = 0;
        cc_block[0].end_frame = -1;
        cc_block[0].type = NONE;
        for (i = 1; i < max_cc_block_count; i++)
        {
            cc_block[i].start_frame = -1;
            cc_block[i].end_frame = -1;
            cc_block[i].type = NONE;
        }

        if(!initialized)
        {
            cc_memory = static_cast<unsigned char **>( malloc(15 * sizeof(unsigned char *)) );
            cc_screen = static_cast<unsigned char **>( malloc(15 * sizeof(unsigned char *)) );
            for (i = 0; i < 15; i++)
            {
                cc_memory[i] = static_cast<unsigned char *>( malloc(32 * sizeof(unsigned char)) );
                cc_screen[i] = static_cast<unsigned char *>( malloc(32 * sizeof(unsigned char)) );
            }
        }
        for(i=0; i<15; i++)
        {
            for (j = 0; j < 32; j++)
            {
                cc_memory[i][j] = 0;
                cc_screen[i][j] = 0;
            }
        }

        if(!initialized)
        {
            max_cc_text_count = 1;
            cc_text = static_cast<cc_text_info *>( malloc((max_cc_text_count + 1) * sizeof(cc_text_info)) );
        }
        if (cc_text == NULL)
        {
            Debug(0, "Could not allocate memory for cc text groups\n");
            comskip::request_exit(22);
        }

        cc_text[0].start_frame = 1;
        cc_text[0].end_frame = -1;
        cc_text[0].text[0] = '\0';
        cc_text[0].text_len = 0;
        for (i = 1; i < max_cc_text_count; i++)
        {
            cc_text[i].start_frame = -1;
            cc_text[i].end_frame = -1;
            cc_text[i].text[0] = '\0';
            cc_text[i].text_len = 0;
        }
    }

//	if (commDetectMethod & AR) {
    if(!initialized)
    {
        max_ar_block_count = 100;
        ar_block = static_cast<ar_block_info *>( malloc((int)((max_ar_block_count + 1) * sizeof(ar_block_info))) );
        max_ac_block_count = 100;
        ac_block = static_cast<ac_block_info *>( malloc((int)((max_ac_block_count + 1) * sizeof(ac_block_info))) );
    }
    if (ar_block == NULL)
    {
        Debug(0, "Could not allocate memory for aspect ratio block array\n");
        comskip::request_exit(31);
    }
    if (ac_block == NULL)
    {
        Debug(0, "Could not allocate memory for audio channel block array\n");
        comskip::request_exit(31);
    }
//	}

    cc.cc1[0] = 0;
    cc.cc1[1] = 0;
    cc.cc2[0] = 0;
    cc.cc2[1] = 0;
    lastcc.cc1[0] = 0;
    lastcc.cc1[1] = 0;
    lastcc.cc2[0] = 0;
    lastcc.cc2[1] = 0;

    Init_XDS_block();

    if (max_avg_brightness == 0)
    {
        if (fps == 25.00)
            max_avg_brightness = 19;
        else
            max_avg_brightness = 19;
    }
    schange_count = 0;
    frame_count	= 0;
    framesprocessed =0;
    black_count = 0;
    block_count = 0;
    ar_block_count = 0;
    ac_block_count = 0;
    framenum_real = 0;
    frames_with_logo = 0;
    framenum = 0;
    lastLogoTest = false;
    commercial_count = -1;

    logoTrendCounter = 0;
//	audio_framenum = 0;
    cc_block_count = 0;
    cc_text_count = 0;
    logo_block_count = 0;
//	pts = 0;
    ascr=scr=0;
    InitScanLines();
    InitHasLogo();
    initialized = true;
    close_dump();
}

