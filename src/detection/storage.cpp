#include "app/debug.h"
#include "recording_context.h"
#include "../localization/diagnostic.h"
#include "storage.h"
#include "buffer_growth.h"
#include "detector_defaults.h"

#include <algorithm>
#include <limits>
#include <string_view>

namespace {

void report_growth(RecordingContext& context, std::string_view message, long capacity)
{
    Debug(context, 9, context.translator.format(message, capacity));
}

}

void InitializeFrameArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (context.state.frame_count > std::numeric_limits<long>::max() - 1000)
        throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::detection_frame_index_exceeds_supported_size);
    if (comskip::detection::grow_buffer(context.state.frame, context.state.max_frame_count,
            std::max(i, context.state.frame_count + 1000), 90000, 1))
        report_growth(context, "storage_resize_frame", context.state.max_frame_count);

    context.state.frame[i] = frame_info{
        .schange_percent = 100,
        .ar_ratio = comskip::detection::undefined_aspect_ratio,
        .xds = i > 0 ? context.state.frame[i - 1].xds : 0,
        .audio_channels = comskip::detection::undefined_audio_channels,
    };
}

void InitializeBlackArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.black, context.state.max_black_count,
            std::max(i, context.state.black_count), 500, 1))
        report_growth(context, "storage_resize_black", context.state.max_black_count);

    context.state.black[i] = black_frame_info{
        .brightness = 255,
        .volume = context.state.curvolume,
    };
}

void InitializeSchangeArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.schange, context.state.max_schange_count,
            std::max(i, context.state.schange_count), 2000, 1))
        report_growth(context, "storage_resize_scene_change", context.state.max_schange_count);

    context.state.schange[i] = schange_info{.frame = i, .percentage = 100};
}

void InitializeLogoBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.logo_block, context.state.max_logo_block_count,
            std::max(i, context.state.logo_block_count), 20, 2))
        report_growth(context, "storage_resize_logo_block", context.state.max_logo_block_count);
    context.state.logo_block[i] = {};
}

void InitializeARBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.ar_block, context.state.max_ar_block_count,
            std::max(i, context.state.ar_block_count), 20, 2))
        report_growth(context, "storage_resize_aspect_block", context.state.max_ar_block_count);
    context.state.ar_block[i] = {};
}

void InitializeACBlockArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.ac_block, context.state.max_ac_block_count,
            std::max(i, context.state.ac_block_count), 20, 2))
        report_growth(context, "storage_resize_audio_block", context.state.max_ac_block_count);
    context.state.ac_block[i] = {};
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
        report_growth(context, "storage_resize_caption_block", context.state.max_cc_block_count);

    context.state.cc_block[i] = cc_block_info{
        .start_frame = -1,
        .end_frame = -1,
        .type = comskip::detection::no_caption_type,
    };
}

void InitializeCCTextArray(RecordingContext& context, long i)
{
    if (i < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
    if (comskip::detection::grow_buffer(context.state.cc_text, context.state.max_cc_text_count,
            std::max(i, context.state.cc_text_count), 100, 1))
        report_growth(context, "storage_resize_caption_text", context.state.max_cc_text_count);

    context.state.cc_text[i] = {};
}
