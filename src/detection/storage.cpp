#include "legacy_detection.h"

void InitializeFrameArray(long i)
{
    if (frame_count+1000 /* max size audio can run ahead of video */ >= max_frame_count)
    {
        max_frame_count += (int)(60 * 60 * 25);
        frame = static_cast<frame_info *>( realloc(frame, max_frame_count * sizeof(frame_info)) );
        Debug(9, "Resizing frame array to accommodate %i frames.\n", max_frame_count);
        if (frame == NULL)
        {
            Debug(0, "Failed to allocated space for the frame array, quitting \n");
            exit(1);
        }
    }

    frame[i].brightness = 0;
//	frame[i].frame = i;
    frame[i].logo_present = false;
    frame[i].schange_percent = 100;
    frame[i].cutscenematch = 0;
#ifdef FRAME_WITH_HISTOGRAM
    memset(frame[i].histogram, 0, sizeof(frame[i].histogram));
#endif
    frame[i].uniform = 0;
    frame[i].ar_ratio = AR_UNDEF;
    frame[i].audio_channels = AC_UNDEF;
    frame[i].minY = 0;
    frame[i].maxY = 0;
    frame[i].isblack = 0;
    if (i > 0)
        frame[i].xds = frame[i-1].xds;
    else
        frame[i].xds = 0;
//	frame[i].volume = curvolume;

}

void InitializeBlackArray(long i)
{
    if (black_count >= max_black_count)
    {
        max_black_count += 500;
        black = static_cast<black_frame_info *>( realloc(black, (max_black_count + 1) * sizeof(black_frame_info)) );
        Debug(9, "Resizing black frame array to accommodate %i frames.\n", max_black_count);
    }

    black[i].brightness = 255;
    black[i].uniform = 0;
    black[i].volume = curvolume;
    //	black[i].frame = i; Wrong!!!!!!!!!!!!!!!!!!!!!!!!!
}

void InitializeSchangeArray(long i)
{
    if (i >= max_schange_count)
    {
        max_schange_count += 2000;
        void *ptr = realloc(schange, (max_schange_count + 1) * sizeof(schange_info));
        if (ptr == NULL) {
            Debug(0, "Could not allocate memory for %i scene change frames.\n", max_schange_count);
            exit(12);
        }
        schange = static_cast<schange_info *>( ptr );
        Debug(9, "Resizing scene change array to accommodate %i frames.\n", max_schange_count);
    }

    schange[i].frame = i;
    schange[i].percentage = 100;
}

void InitializeLogoBlockArray(long i)
{
    if (logo_block_count >= max_logo_block_count)
    {
        max_logo_block_count += 20;
        logo_block = static_cast<logo_block_info *>( realloc(logo_block, (max_logo_block_count + 2) * sizeof(logo_block_info)) );
        Debug(9, "Resizing logo cblock array to accommodate %i logo groups.\n", max_logo_block_count);
    }
}

void InitializeARBlockArray(long i)
{
    if (ar_block_count >= max_ar_block_count)
    {
        max_ar_block_count += 20;
        ar_block = static_cast<ar_block_info *>( realloc(ar_block, (max_ar_block_count + 2) * sizeof(ar_block_info)) );
        Debug(9, "Resizing aspect ratio cblock array to accommodate %i AR groups.\n", max_ar_block_count);
    }
}

void InitializeACBlockArray(long i)
{
    if (ac_block_count >= max_ac_block_count)
    {
        max_ac_block_count += 20;
        ac_block = static_cast<ac_block_info *>( realloc(ac_block, (max_ac_block_count + 2) * sizeof(ac_block_info)) );
        Debug(9, "Resizing audio channel block array to accommodate %i AC groups.\n", max_ac_block_count);
    }
}

void InitializeBlockArray(long i)
{
    if (block_count >= max_block_count)
    {
        Debug(0,"Panic, too many blocks\n");
        exit(102);
//		max_block_count += 100;
//		cblock = realloc(cblock, (max_block_count + 1) * sizeof(block_info));
//		Debug(9, "Resizing cblock array to accommodate %i blocks.\n", max_block_count);
    }

    cblock[i].f_start = 0;
    cblock[i].f_end = 0;
    cblock[i].b_head = 0;
    cblock[i].b_tail = 0;
    cblock[i].bframe_count = 0;
    cblock[i].schange_count = 0;
    cblock[i].schange_rate = 0;
    cblock[i].length = 0;
    cblock[i].score = 1.0;
    cblock[i].combined_count = 0;
    cblock[i].ar_ratio = AR_UNDEF;
    cblock[i].audio_channels = AC_UNDEF;
    cblock[i].brightness = 0;
    cblock[i].volume = 0;
    cblock[i].silence = 0;
    cblock[i].stdev = 0;
    cblock[i].cause = 0;
    cblock[i].less = 0;
    cblock[i].more = 0;
    cblock[i].uniform = 0;
}

void InitializeCCBlockArray(long i)
{
    if (cc_block_count >= max_cc_block_count)
    {
        max_cc_block_count += 100;
        cc_block = static_cast<cc_block_info *>( realloc(cc_block, (max_cc_block_count + 2) * sizeof(cc_block_info)) );
        Debug(9, "Resizing cc cblock array to accommodate %i cc blocks.\n", max_cc_block_count);
    }

    cc_block[i].start_frame = -1;
    cc_block[i].end_frame = -1;
    cc_block[i].type = NONE;
}

void InitializeCCTextArray(long i)
{
    if (cc_text_count >= max_cc_text_count)
    {
        max_cc_text_count += 100;
        cc_text = static_cast<cc_text_info *>( realloc(cc_text, (max_cc_text_count + 1) * sizeof(cc_text_info)) );
        Debug(9, "Resizing cc text array to accommodate %i cc text groups.\n", max_cc_text_count);
    }

    cc_text[i].text[0] = '\0';
    cc_text[i].text_len = 0;
}

