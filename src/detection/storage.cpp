#include "exit_requested.h"
#include "legacy_detection.h"
#include "buffer_growth.h"

void InitializeFrameArray(RecordingContext& context, long i)
{
    if (context.state.frame_count > std::numeric_limits<long>::max() - 1000)
        throw std::length_error("Detection frame index exceeds supported size");
    if (comskip::detection::grow_buffer(context.state.frame, context.state.max_frame_count,
            std::max(i, context.state.frame_count + 1000), 90000, 1))
        Debug(context, 9, "Resizing frame buffer to accommodate %li entries.\n", context.state.max_frame_count);

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
    if (comskip::detection::grow_buffer(context.state.black, context.state.max_black_count,
            std::max(i, context.state.black_count), 500, 1))
        Debug(context, 9, "Resizing black buffer to accommodate %li entries.\n", context.state.max_black_count);

    context.state.black[i].brightness = 255;
    context.state.black[i].uniform = 0;
    context.state.black[i].volume = context.state.curvolume;
    //	black[i].frame = i; Wrong!!!!!!!!!!!!!!!!!!!!!!!!!
}

void InitializeSchangeArray(RecordingContext& context, long i)
{
    if (comskip::detection::grow_buffer(context.state.schange, context.state.max_schange_count,
            std::max(i, context.state.schange_count), 2000, 1))
        Debug(context, 9, "Resizing schange buffer to accommodate %li entries.\n", context.state.max_schange_count);

    context.state.schange[i].frame = i;
    context.state.schange[i].percentage = 100;
}

void InitializeLogoBlockArray(RecordingContext& context, long i)
{
    if (comskip::detection::grow_buffer(context.state.logo_block, context.state.max_logo_block_count,
            std::max(i, context.state.logo_block_count), 20, 2))
        Debug(context, 9, "Resizing logo_block buffer to accommodate %li entries.\n", context.state.max_logo_block_count);

}

void InitializeARBlockArray(RecordingContext& context, long i)
{
    if (comskip::detection::grow_buffer(context.state.ar_block, context.state.max_ar_block_count,
            std::max(i, context.state.ar_block_count), 20, 2))
        Debug(context, 9, "Resizing ar_block buffer to accommodate %li entries.\n", context.state.max_ar_block_count);

}

void InitializeACBlockArray(RecordingContext& context, long i)
{
    if (comskip::detection::grow_buffer(context.state.ac_block, context.state.max_ac_block_count,
            std::max(i, context.state.ac_block_count), 20, 2))
        Debug(context, 9, "Resizing ac_block buffer to accommodate %li entries.\n", context.state.max_ac_block_count);

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
    if (comskip::detection::grow_buffer(context.state.cc_block, context.state.max_cc_block_count,
            std::max(i, context.state.cc_block_count), 100, 2))
        Debug(context, 9, "Resizing cc_block buffer to accommodate %li entries.\n", context.state.max_cc_block_count);

    context.state.cc_block[i].start_frame = -1;
    context.state.cc_block[i].end_frame = -1;
    context.state.cc_block[i].type = NONE;
}

void InitializeCCTextArray(RecordingContext& context, long i)
{
    if (comskip::detection::grow_buffer(context.state.cc_text, context.state.max_cc_text_count,
            std::max(i, context.state.cc_text_count), 100, 1))
        Debug(context, 9, "Resizing cc_text buffer to accommodate %li entries.\n", context.state.max_cc_text_count);

    context.state.cc_text[i].text[0] = '\0';
    context.state.cc_text[i].text_len = 0;
}

