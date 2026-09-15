#include "exit_requested.h"
#include "legacy_detection.h"

void InitializeFrameArray(RecordingContext& context, long i)
{
    if (context.state.frame_count+1000 /* max size audio can run ahead of video */ >= context.state.max_frame_count)
    {
        context.state.max_frame_count += (int)(60 * 60 * 25);
        context.state.frame = static_cast<frame_info *>( realloc(context.state.frame, context.state.max_frame_count * sizeof(frame_info)) );
        Debug(context, 9, "Resizing frame array to accommodate %i frames.\n", context.state.max_frame_count);
        if (context.state.frame == NULL)
        {
            Debug(context, 0, "Failed to allocated space for the frame array, quitting \n");
            comskip::request_exit(1);
        }
    }

    context.state.frame[i].brightness = 0;
//	frame[i].frame = i;
    context.state.frame[i].logo_present = false;
    context.state.frame[i].schange_percent = 100;
    context.state.frame[i].cutscenematch = 0;
#ifdef FRAME_WITH_HISTOGRAM
    memset(context.state.frame[i].histogram, 0, sizeof(context.state.frame[i].histogram));
#endif
    context.state.frame[i].uniform = 0;
    context.state.frame[i].ar_ratio = AR_UNDEF;
    context.state.frame[i].audio_channels = AC_UNDEF;
    context.state.frame[i].minY = 0;
    context.state.frame[i].maxY = 0;
    context.state.frame[i].isblack = 0;
    if (i > 0)
        context.state.frame[i].xds = context.state.frame[i-1].xds;
    else
        context.state.frame[i].xds = 0;
//	frame[i].volume = curvolume;

}

void InitializeBlackArray(RecordingContext& context, long i)
{
    if (context.state.black_count >= context.state.max_black_count)
    {
        context.state.max_black_count += 500;
        context.state.black = static_cast<black_frame_info *>( realloc(context.state.black, (context.state.max_black_count + 1) * sizeof(black_frame_info)) );
        Debug(context, 9, "Resizing black frame array to accommodate %i frames.\n", context.state.max_black_count);
    }

    context.state.black[i].brightness = 255;
    context.state.black[i].uniform = 0;
    context.state.black[i].volume = context.state.curvolume;
    //	black[i].frame = i; Wrong!!!!!!!!!!!!!!!!!!!!!!!!!
}

void InitializeSchangeArray(RecordingContext& context, long i)
{
    if (i >= context.state.max_schange_count)
    {
        context.state.max_schange_count += 2000;
        void *ptr = realloc(context.state.schange, (context.state.max_schange_count + 1) * sizeof(schange_info));
        if (ptr == NULL) {
            Debug(context, 0, "Could not allocate memory for %i scene change frames.\n", context.state.max_schange_count);
            comskip::request_exit(12);
        }
        context.state.schange = static_cast<schange_info *>( ptr );
        Debug(context, 9, "Resizing scene change array to accommodate %i frames.\n", context.state.max_schange_count);
    }

    context.state.schange[i].frame = i;
    context.state.schange[i].percentage = 100;
}

void InitializeLogoBlockArray(RecordingContext& context, long i)
{
    if (context.state.logo_block_count >= context.state.max_logo_block_count)
    {
        context.state.max_logo_block_count += 20;
        context.state.logo_block = static_cast<logo_block_info *>( realloc(context.state.logo_block, (context.state.max_logo_block_count + 2) * sizeof(logo_block_info)) );
        Debug(context, 9, "Resizing logo cblock array to accommodate %i logo groups.\n", context.state.max_logo_block_count);
    }
}

void InitializeARBlockArray(RecordingContext& context, long i)
{
    if (context.state.ar_block_count >= context.state.max_ar_block_count)
    {
        context.state.max_ar_block_count += 20;
        context.state.ar_block = static_cast<ar_block_info *>( realloc(context.state.ar_block, (context.state.max_ar_block_count + 2) * sizeof(ar_block_info)) );
        Debug(context, 9, "Resizing aspect ratio cblock array to accommodate %i AR groups.\n", context.state.max_ar_block_count);
    }
}

void InitializeACBlockArray(RecordingContext& context, long i)
{
    if (context.state.ac_block_count >= context.state.max_ac_block_count)
    {
        context.state.max_ac_block_count += 20;
        context.state.ac_block = static_cast<ac_block_info *>( realloc(context.state.ac_block, (context.state.max_ac_block_count + 2) * sizeof(ac_block_info)) );
        Debug(context, 9, "Resizing audio channel block array to accommodate %i AC groups.\n", context.state.max_ac_block_count);
    }
}

void InitializeBlockArray(RecordingContext& context, long i)
{
    if (context.state.block_count >= context.state.max_block_count)
    {
        Debug(context, 0,"Panic, too many blocks\n");
        comskip::request_exit(102);
//		max_block_count += 100;
//		cblock = realloc(cblock, (max_block_count + 1) * sizeof(block_info));
//		Debug(9, "Resizing cblock array to accommodate %i blocks.\n", max_block_count);
    }

    context.state.cblock[i].f_start = 0;
    context.state.cblock[i].f_end = 0;
    context.state.cblock[i].b_head = 0;
    context.state.cblock[i].b_tail = 0;
    context.state.cblock[i].bframe_count = 0;
    context.state.cblock[i].schange_count = 0;
    context.state.cblock[i].schange_rate = 0;
    context.state.cblock[i].length = 0;
    context.state.cblock[i].score = 1.0;
    context.state.cblock[i].combined_count = 0;
    context.state.cblock[i].ar_ratio = AR_UNDEF;
    context.state.cblock[i].audio_channels = AC_UNDEF;
    context.state.cblock[i].brightness = 0;
    context.state.cblock[i].volume = 0;
    context.state.cblock[i].silence = 0;
    context.state.cblock[i].stdev = 0;
    context.state.cblock[i].cause = 0;
    context.state.cblock[i].less = 0;
    context.state.cblock[i].more = 0;
    context.state.cblock[i].uniform = 0;
}

void InitializeCCBlockArray(RecordingContext& context, long i)
{
    if (context.state.cc_block_count >= context.state.max_cc_block_count)
    {
        context.state.max_cc_block_count += 100;
        context.state.cc_block = static_cast<cc_block_info *>( realloc(context.state.cc_block, (context.state.max_cc_block_count + 2) * sizeof(cc_block_info)) );
        Debug(context, 9, "Resizing cc cblock array to accommodate %i cc blocks.\n", context.state.max_cc_block_count);
    }

    context.state.cc_block[i].start_frame = -1;
    context.state.cc_block[i].end_frame = -1;
    context.state.cc_block[i].type = NONE;
}

void InitializeCCTextArray(RecordingContext& context, long i)
{
    if (context.state.cc_text_count >= context.state.max_cc_text_count)
    {
        context.state.max_cc_text_count += 100;
        context.state.cc_text = static_cast<cc_text_info *>( realloc(context.state.cc_text, (context.state.max_cc_text_count + 1) * sizeof(cc_text_info)) );
        Debug(context, 9, "Resizing cc text array to accommodate %i cc text groups.\n", context.state.max_cc_text_count);
    }

    context.state.cc_text[i].text[0] = '\0';
    context.state.cc_text[i].text_len = 0;
}

