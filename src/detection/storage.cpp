#include "../localization/diagnostic.h"
#include "exit_requested.h"
#include "storage.h"
#include "legacy_detection.h"
#include "buffer_growth.h"

void InitializeFrameArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (context.state.frame_count > std::numeric_limits<long>::max() - 1000)
        throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::detection_frame_index_exceeds_supported_size);
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
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
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
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.schange, context.state.max_schange_count,
            std::max(i, context.state.schange_count), 2000, 1))
        Debug(context, 9, "Resizing schange buffer to accommodate %li entries.\n", context.state.max_schange_count);

    context.state.schange[i].frame = i;
    context.state.schange[i].percentage = 100;
}

void InitializeLogoBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.logo_block, context.state.max_logo_block_count,
            std::max(i, context.state.logo_block_count), 20, 2))
        Debug(context, 9, "Resizing logo_block buffer to accommodate %li entries.\n", context.state.max_logo_block_count);

}

void InitializeARBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.ar_block, context.state.max_ar_block_count,
            std::max(i, context.state.ar_block_count), 20, 2))
        Debug(context, 9, "Resizing ar_block buffer to accommodate %li entries.\n", context.state.max_ar_block_count);

}

void InitializeACBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.ac_block, context.state.max_ac_block_count,
            std::max(i, context.state.ac_block_count), 20, 2))
        Debug(context, 9, "Resizing ac_block buffer to accommodate %li entries.\n", context.state.max_ac_block_count);

}

void InitializeBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (i < 0 || static_cast<std::size_t>(i) >= context.state.cblock.size())
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::detection_block_initialization_exceeds_owned_storage);
    context.state.cblock[i] = comskip::detection::empty_block();
}

void InitializeCCBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.cc_block, context.state.max_cc_block_count,
            std::max(i, context.state.cc_block_count), 100, 2))
        Debug(context, 9, "Resizing cc_block buffer to accommodate %li entries.\n", context.state.max_cc_block_count);

    context.state.cc_block[i].start_frame = -1;
    context.state.cc_block[i].end_frame = -1;
    context.state.cc_block[i].type = NONE;
}

void InitializeCCTextArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.cc_text, context.state.max_cc_text_count,
            std::max(i, context.state.cc_text_count), 100, 1))
        Debug(context, 9, "Resizing cc_text buffer to accommodate %li entries.\n", context.state.max_cc_text_count);

    context.state.cc_text[i].text[0] = '\0';
    context.state.cc_text[i].text_len = 0;
}

