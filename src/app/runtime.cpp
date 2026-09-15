#include "exit_requested.h"
#include "legacy_detection.h"

int CountSceneChanges(RecordingContext& context, int StartFrame, int EndFrame)
{
    int i;
    double p = 0;
    int count = 0;
    for (i = 0; i < context.state.schange_count; i++)
    {
        if ((context.state.schange[i].frame > StartFrame) && (context.state.schange[i].frame < EndFrame))
        {
            count++;
            p += (double)(100 - context.state.schange[i].percentage)  / (100 - context.state.schange_threshold);
        }
    }
    count = (int) p;

    return (count);
}

void Debug(RecordingContext& context, int level, const char * fmt, ...)
{
    va_list	ap;
    FILE *log_file = NULL;
    if(context.settings.verbose < level) return;

    va_start(ap, fmt);
    vsnprintf(context.state.debugText, sizeof(context.state.debugText), fmt, ap);
    va_end(ap);

    if (context.state.output_console)	_cprintf("%s", context.state.debugText);

    if (!log_file)
        log_file = myfopen(context.state.logfilename, "a+");

    if (log_file)
    {
        fprintf(log_file, "%s", context.state.debugText);
        fclose(log_file);
        log_file = NULL;
    }


    context.state.debugText[0] = '\0';
}

void InitLogoBuffers(RecordingContext& context)
{
    int i;
    if(!context.state.logoFrameNum) context.state.logoFrameNum = static_cast<int *>( malloc(context.settings.num_logo_buffers * sizeof(int)) );
    if (context.state.logoFrameNum == NULL)
    {
        Debug(context, 0, "Could not allocate memory for logo buffer frame number array\n");
        comskip::request_exit(14);
    }
    memset(context.state.logoFrameNum, 0,context.settings.num_logo_buffers*sizeof(int));
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
    if(!context.state.logoFrameBuffer)
    {
        context.state.logoFrameBuffer = static_cast<unsigned char **>( malloc(context.settings.num_logo_buffers * sizeof(unsigned char *)) );
        if (!(context.state.logoFrameBuffer == NULL))
        {

            context.state.lheight = MAXHEIGHT;
            context.state.lwidth = MAXWIDTH;
            context.state.logoFrameBufferSize = context.state.lwidth * context.state.lheight * sizeof(context.state.frame_ptr[0]);
            for (i = 0; i < context.settings.num_logo_buffers; i++)
            {
                context.state.logoFrameBuffer[i] = static_cast<unsigned char *>( malloc(context.state.logoFrameBufferSize) );
                if (context.state.logoFrameBuffer[i] == NULL)
                {
                    Debug(context, 0, "Could not allocate memory for logo frame buffer %i\n", i);
                    comskip::request_exit(16);
                }
            }
        }
        else
        {
            Debug(context, 0, "Could not allocate memory for logo frame buffers\n");
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

void Init_XDS_block(RecordingContext& context);

void InitComSkip(RecordingContext& context)
{
    int i, j;
    context.state.min_brightness_found = 255;
    context.state.max_logo_gap = -1;
    context.state.max_nonlogo_block_length = -1;
    context.state.logo_overshoot = 0.0;
    for (i = 0; i < 256; i++) context.state.brightHistogram[i] = 0;
    for (i = 0; i < 256; i++) context.state.uniformHistogram[i] = 0;
    for (i = 0; i < 256; i++) context.state.volumeHistogram[i] = 0;
    for (i = 0; i < 256; i++) context.state.silenceHistogram[i] = 0;

    if (context.state.framearray)
    {
        if(!context.state.initialized)
        {
            context.state.max_frame_count = (int)(60 * 60 * context.settings.fps) + 1;
            context.state.frame = static_cast<frame_info *>( malloc((int)((context.state.max_frame_count + 1) * sizeof(frame_info))) );
        }
        if (context.state.frame == NULL)
        {
            Debug(context, 0, "Could not allocate memory for frame array\n");
            comskip::request_exit(10);
        }
    }

//	if (commDetectMethod & BLACK_FRAME) {
    if(!context.state.initialized)
    {
        context.state.max_black_count = 500;
        context.state.black = static_cast<black_frame_info *>( malloc((int)((context.state.max_black_count + 1) * sizeof(black_frame_info))) );
    }
    if (context.state.black == NULL)
    {
        Debug(context, 0, "Could not allocate memory for black frame array\n");
        comskip::request_exit(11);
    }
//	} else {
//		Debug(1, "ERROR: ComSkip cannot run without black frames.\n");
//		comskip::request_exit(100);
//	}

    if (context.settings.commDetectMethod & LOGO)
    {
        if(!context.state.initialized)
        {
            context.state.max_logo_block_count = 1000;
            context.state.logo_block = static_cast<logo_block_info *>( malloc((int)((context.state.max_logo_block_count + 1) * sizeof(logo_block_info))) );
        }
        if (context.state.logo_block == NULL)
        {
            Debug(context, 0, "Could not allocate memory for logo cblock array\n");
            comskip::request_exit(13);
        }

//		if (!logoInfoAvailable) {
        InitLogoBuffers(context);
//		}
        memset(context.state.max_br,   0, sizeof(context.state.max_br));
        memset(context.state.min_br, 255, sizeof(context.state.min_br));
    }

    if (context.settings.commDetectMethod & SCENE_CHANGE)
    {
        if(!context.state.initialized)
        {
            context.state.max_schange_count = 2000;
            context.state.schange = static_cast<schange_info *>( malloc((int)((context.state.max_schange_count + 1) * sizeof(schange_info))) );
        }
        if (context.state.schange == NULL)
        {
            Debug(context, 0, "Could not allocate memory for scene change array\n");
            comskip::request_exit(12);
        }
    }

    if (context.state.processCC)
    {
        if(!context.state.initialized)
        {
            context.state.max_cc_block_count = 500;
            context.state.cc_block = static_cast<cc_block_info *>( malloc((context.state.max_cc_block_count + 1) * sizeof(cc_block_info)) );
        }
        if (context.state.cc_block == NULL)
        {
            Debug(context, 0, "Could not allocate memory for cc blocks\n");
            comskip::request_exit(22);
        }

        context.state.cc_block[0].start_frame = 0;
        context.state.cc_block[0].end_frame = -1;
        context.state.cc_block[0].type = NONE;
        for (i = 1; i < context.state.max_cc_block_count; i++)
        {
            context.state.cc_block[i].start_frame = -1;
            context.state.cc_block[i].end_frame = -1;
            context.state.cc_block[i].type = NONE;
        }

        if(!context.state.initialized)
        {
            context.state.cc_memory = static_cast<unsigned char **>( malloc(15 * sizeof(unsigned char *)) );
            context.state.cc_screen = static_cast<unsigned char **>( malloc(15 * sizeof(unsigned char *)) );
            for (i = 0; i < 15; i++)
            {
                context.state.cc_memory[i] = static_cast<unsigned char *>( malloc(32 * sizeof(unsigned char)) );
                context.state.cc_screen[i] = static_cast<unsigned char *>( malloc(32 * sizeof(unsigned char)) );
            }
        }
        for(i=0; i<15; i++)
        {
            for (j = 0; j < 32; j++)
            {
                context.state.cc_memory[i][j] = 0;
                context.state.cc_screen[i][j] = 0;
            }
        }

        if(!context.state.initialized)
        {
            context.state.max_cc_text_count = 1;
            context.state.cc_text = static_cast<cc_text_info *>( malloc((context.state.max_cc_text_count + 1) * sizeof(cc_text_info)) );
        }
        if (context.state.cc_text == NULL)
        {
            Debug(context, 0, "Could not allocate memory for cc text groups\n");
            comskip::request_exit(22);
        }

        context.state.cc_text[0].start_frame = 1;
        context.state.cc_text[0].end_frame = -1;
        context.state.cc_text[0].text[0] = '\0';
        context.state.cc_text[0].text_len = 0;
        for (i = 1; i < context.state.max_cc_text_count; i++)
        {
            context.state.cc_text[i].start_frame = -1;
            context.state.cc_text[i].end_frame = -1;
            context.state.cc_text[i].text[0] = '\0';
            context.state.cc_text[i].text_len = 0;
        }
    }

//	if (commDetectMethod & AR) {
    if(!context.state.initialized)
    {
        context.state.max_ar_block_count = 100;
        context.state.ar_block = static_cast<ar_block_info *>( malloc((int)((context.state.max_ar_block_count + 1) * sizeof(ar_block_info))) );
        context.state.max_ac_block_count = 100;
        context.state.ac_block = static_cast<ac_block_info *>( malloc((int)((context.state.max_ac_block_count + 1) * sizeof(ac_block_info))) );
    }
    if (context.state.ar_block == NULL)
    {
        Debug(context, 0, "Could not allocate memory for aspect ratio block array\n");
        comskip::request_exit(31);
    }
    if (context.state.ac_block == NULL)
    {
        Debug(context, 0, "Could not allocate memory for audio channel block array\n");
        comskip::request_exit(31);
    }
//	}

    context.state.cc.cc1[0] = 0;
    context.state.cc.cc1[1] = 0;
    context.state.cc.cc2[0] = 0;
    context.state.cc.cc2[1] = 0;
    context.state.lastcc.cc1[0] = 0;
    context.state.lastcc.cc1[1] = 0;
    context.state.lastcc.cc2[0] = 0;
    context.state.lastcc.cc2[1] = 0;

    Init_XDS_block(context);

    if (context.settings.max_avg_brightness == 0)
    {
        if (context.settings.fps == 25.00)
            context.settings.max_avg_brightness = 19;
        else
            context.settings.max_avg_brightness = 19;
    }
    context.state.schange_count = 0;
    context.state.frame_count	= 0;
    context.state.framesprocessed =0;
    context.state.black_count = 0;
    context.state.block_count = 0;
    context.state.ar_block_count = 0;
    context.state.ac_block_count = 0;
    context.state.framenum_real = 0;
    context.state.frames_with_logo = 0;
    context.state.framenum = 0;
    context.state.lastLogoTest = false;
    context.state.commercial_count = -1;

    context.state.logoTrendCounter = 0;
//	audio_framenum = 0;
    context.state.cc_block_count = 0;
    context.state.cc_text_count = 0;
    context.state.logo_block_count = 0;
//	pts = 0;
    context.state.ascr=context.state.scr=0;
    InitScanLines(context);
    InitHasLogo(context);
    context.state.initialized = true;
    close_dump(context);
}

