#include "platform/utf8_paths.h"
#include "platform/platform.h"
#include "app/debug.h"
#include "app/recording_context.h"
#include "config/legacy_settings.h"
#include "detection/captions.h"
#include "detection/caption_observations.h"
#include "detection/detection_methods.h"
#include "detection/frame_causes.h"
#include "detection/frame_timestamps.h"
#include "detection/logo_detection.h"
#include "cutlist_exports.h"
#include "checked_format.h"
#include "xml_output_adapter.h"
#include "ffmpeg_sidecar_adapter.h"
#include "frame_script_adapter.h"
#include "player_export_adapter.h"
#include "legacy_editor_adapter.h"
#include "legacy_cutlist_adapter.h"
#include "csv_field.h"
#include "checked_file.h"
#include "edl.h"
#include "diagnostics.h"
#include <algorithm>
#include <format>
#include <sstream>
#include <string_view>
#include <vector>

namespace {
constexpr std::string_view training_layout =
    "%3d,%c,%c,%7.2f,%7.2f,%7.2f,%7.2f,%7.2f,%5.2f,%5.2f,\"%10s\",\"%10s\",\"%10s\",%s\n";

double frame_time(RecordingContext& context, const long frame) {
    return get_frame_pts(context, static_cast<int>(frame));
}

double frame_duration(RecordingContext& context, const long end_frame, const long start_frame) {
    return frame_time(context, end_frame) - frame_time(context, start_frame);
}

long frame_number(RecordingContext& context, const long frame) {
    return static_cast<long>(frame_time(context, frame) * context.settings.fps + 1.5);
}

comskip::platform::FilePtr open_checked_file(std::string_view path, const char* mode) {
    auto file=comskip::platform::open_file_owned(path,mode);
    if (!file)
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_open,{std::string(path)});
    return file;
}

void append_edl_record(RecordingContext& context, FILE* destination, long start, long end,
                       comskip::output::EdlVariant variant, std::string_view path)
{
    using namespace comskip::output;
    const OutputOptions options{context.settings.edl_offset, context.settings.edl_skip_field,
                                context.state.demux_pid && context.settings.enable_mencoder_pts, variant};
    std::vector<Seconds> timestamps;
    MediaDescription media{context.settings.fps};
    if (!context.state.frame.empty() && context.state.frame_count > 1) {
        auto first = static_cast<FrameIndex>(start < 5 ? 0 : start);
        auto last = static_cast<FrameIndex>(end);
        if (variant == EdlVariant::standard) {
            first = std::max<FrameIndex>(first - options.frame_offset, 0);
            last = std::max<FrameIndex>(last - options.frame_offset, 0);
        }
        first = std::clamp<FrameIndex>(first, 1, context.state.frame_count - 1);
        last = std::clamp<FrameIndex>(last, 1, context.state.frame_count - 1);
        timestamps.reserve(static_cast<std::size_t>(last - first + 1));
        for (auto index = first; index <= last; ++index)
            timestamps.emplace_back(context.state.frame[index].pts);
        media.timestamps = timestamps;
        media.first_frame = first;
        media.first_frame_timestamp = Seconds{get_frame_pts(context, 1)};
    }
    const CommercialInterval interval{start, end};
    std::ostringstream serialized;
    write_edl(serialized, std::span{&interval, 1}, media, options);
    const auto text = serialized.str();
    if (fwrite(text.data(), 1, text.size(), destination) != text.size())
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_write,{std::string(path)});
}
}


void OpenOutputFiles(RecordingContext& context)
{

    if (context.settings.output_default)
    {
        context.state.out_file = comskip::platform::open_file_owned(context.state.out_filename, "w");
        if (!context.state.out_file.get())
        {
            sleep_for_ms(50L);
            context.state.out_file = comskip::platform::open_file_owned(context.state.out_filename, "w");
            if (!context.state.out_file.get())
            {
                throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
                    comskip::diagnostics::Code::output_open,{context.state.out_filename});
            }
        }
        comskip::output::checked_fprintf(*context.state.out_file,context.state.out_filename,
            "FILE PROCESSING COMPLETE %6li FRAMES AT %5i\n-------------------\n",frame_number(context, context.state.frame_count-1),
            static_cast<int>(context.settings.fps * 100));
        comskip::output::checked_close(context.state.out_file,context.state.out_filename);
    }

    if (context.settings.output_incommercial)
    {
        context.state.filename = std::string(context.state.workbasename) + ".incommercial";
        context.state.incommercial_file = comskip::platform::open_file_owned(context.state.filename, "w");
        if (!context.state.incommercial_file.get())
        {
            throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
                comskip::diagnostics::Code::output_open,{context.state.filename});
        }
        comskip::output::checked_fprintf(*context.state.incommercial_file,context.state.filename,"0\n");
        comskip::output::checked_close(context.state.incommercial_file,context.state.filename);
    }




    if (context.settings.output_edl)
    {
        context.state.filename = std::string(context.state.outbasename) + ".edl";
        context.state.edl_file = comskip::platform::open_file_owned(context.state.filename, "wb");
        if (!context.state.edl_file.get())
        {
            throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
                comskip::diagnostics::Code::output_open,{context.state.filename});
        }
    }
}

void OutputCommercialBlock(RecordingContext& context, int i, long prev, long start, long end, bool last)
{
    int s_start, s_end;
    int count;
    double minutes = frame_time(context, context.state.frame_count)/60;

/*
    // Convert from frame array index to (timecode / fps) for external output
    if (prev > 0)
        prev = frame_number(context, prev);
    if (start > 0 && start <= frame_count)
        start = frame_number(context, start);
    if (end > 0 && end <= frame_count)
        end = frame_number(context, end);

    start = std::max(start,0);
    end = std::max(end,0);
*/

    s_start = start;
    s_end = end;

    if (context.settings.sage_minute_bug)
    {
        s_start = static_cast<int>(start * (static_cast<int>(minutes + 0.5) / minutes));
        s_end = static_cast<int>(end * (static_cast<int>(minutes + 0.5) / minutes));
    }
    if (context.settings.output_default && prev < start /*&& !last */)
    {
        context.state.out_file = comskip::platform::open_file_owned(context.state.out_filename, "a+");
        if (context.state.out_file.get())
        {
            comskip::output::checked_fprintf(*context.state.out_file,context.state.out_filename,"%li\t%li\n", frame_number(context, context.settings.sage_framenumber_bug?s_start/2:s_start), frame_number(context, context.settings.sage_framenumber_bug?s_end/2:s_end));
            comskip::output::checked_close(context.state.out_file,context.state.out_filename);
        }
        else  		// If the file can't be opened for writting, wait half a second and try again
        {
            sleep_for_ms(50L);
            context.state.out_file = comskip::platform::open_file_owned(context.state.out_filename, "a+");
            if (context.state.out_file.get())
            {
                comskip::output::checked_fprintf(*context.state.out_file,context.state.out_filename,"%li\t%li\n", frame_number(context, context.settings.sage_framenumber_bug?s_start/2:s_start), frame_number(context, context.settings.sage_framenumber_bug?s_end/2:s_end));
                comskip::output::checked_close(context.state.out_file,context.state.out_filename);
            }
            else  	// If the file still can't be opened for writting, give up and exit
            {
                throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
                    comskip::diagnostics::Code::output_open,{context.state.out_filename});
            }
        }
    }
    //CLOSEOUTFILE(context.state.out_file);

    if (context.state.edl_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        append_edl_record(context, context.state.edl_file.get(), start < 5 ? 0 : start, end, comskip::output::EdlVariant::standard,
                          std::string(context.state.outbasename)+".edl");
    }
    if (last) comskip::output::checked_close(context.state.edl_file,std::string(context.state.outbasename)+".edl");

    if (context.state.live_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        append_edl_record(context, context.state.live_file.get(), start < 5 ? 0 : start, end, comskip::output::EdlVariant::standard,
                          std::string(context.state.outbasename)+".live");
    }
    if (last) comskip::output::checked_close(context.state.live_file,std::string(context.state.outbasename)+".live");

    if (context.state.edlp_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        append_edl_record(context, context.state.edlp_file.get(), start < 5 ? 0 : start, end, comskip::output::EdlVariant::plus,
                          std::string(context.state.outbasename)+".edlp");
    }
    if (last) comskip::output::checked_close(context.state.edlp_file,std::string(context.state.outbasename)+".edlp");

}

char CompareLetter(RecordingContext& context, int value, int average, int i)
{
    if (context.state.cblock[i].reffer == '+' || context.state.cblock[i].reffer == '-')
    {
        if (value > 1.2 * average)
        {
            if (context.state.cblock[i].reffer == '-')
                return('=');
            else
                return('!');
        }
        if (value < 0.8 * average)
        {
            if (context.state.cblock[i].reffer == '-')
                return('!');
            else
                return('=');
        }
    }
    if (value > average)
    {
        return('+');
    }
    if (value < average)
    {
        return('-');
    }
    return('0');

}

void BuildCommercial(RecordingContext& context)
{
    if (context.state.block_count < 0 || context.state.block_count > std::numeric_limits<int>::max() ||
        static_cast<std::size_t>(context.state.block_count) + 1 != context.state.cblock.size())
        throw std::out_of_range("Commercial producer count exceeds owned detection blocks");
    std::vector<Legacy_commercial_entry> intervals;
    int last = -1;
    for (int i = 0; i < context.state.block_count; ++i)
    {
        const auto& block = context.state.cblock[i];
        if (!(block.score > context.settings.global_threshold)) continue;
        if (i == 0 || !(context.state.cblock[i - 1].score > context.settings.global_threshold))
            comskip::detection::append_interval(intervals, last,
                Legacy_commercial_entry{block.f_start, block.f_end, i, i, frame_duration(context, block.f_end, block.f_start)});
        else {
            auto& interval = intervals.back();
            interval.end_frame = block.f_end;
            interval.end_block = i;
            interval.length = frame_duration(context, interval.end_frame, interval.start_frame);
        }
    }
    context.state.commercial = std::move(intervals);
    context.state.commercial_count = last;
    for (int i = 0; i < context.state.block_count; ++i)
        context.state.cblock[i].iscommercial = context.state.cblock[i].score > context.settings.global_threshold;
}


bool OutputBlocks(RecordingContext& context)
{
    int		i,k;
    long	prev;
    double comlength;
    double	threshold;
    bool	foundCommercials = false;
    bool	deleted = false;

    if (context.settings.global_threshold >= 0.0)
    {
        threshold = context.settings.global_threshold;
    }
    else
    {
        threshold = FindScoreThreshold(context, context.settings.score_percentile);
    }

    OpenOutputFiles(context);


    Debug(context, 1, context.translator.format("cutlists_threshold_used",
        std::format("{:.4f}", threshold)));
    threshold = ceil(threshold * 100) / 100.0;
    Debug(context, 1, context.translator.format("cutlists_threshold_rounded",
        std::format("{:.4f}", threshold)));

    BuildCommercial(context);

#ifdef undef
    context.state.commercial_count = -1;
    i = 0;
    while (i < context.state.block_count)
    {
        if (context.state.cblock[i].score > threshold
//			&&
//			( cblock[i].score >= 100 ||
//			!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && frame_duration(context, cblock[i].f_end, cblock[i].f_start) > (min_show_segment_length) ))
           )
        {
            context.state.commercial_count++;
            context.state.commercial[context.state.commercial_count].start_frame = context.state.cblock[i].f_start/*+ (cblock[i].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].length = frame_duration(context, context.state.commercial[context.state.commercial_count].end_frame,	context.state.commercial[context.state.commercial_count].start_frame);
            context.state.commercial[context.state.commercial_count].start_block = i;
            context.state.commercial[context.state.commercial_count].end_block = i;
            context.state.cblock[i].iscommercial = true;
            i++;
            while (i < context.state.block_count && context.state.cblock[i].score > threshold
//				&&
//				( cblock[i].score >= 100 ||
//				!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && frame_duration(context, cblock[i].f_end, cblock[i].f_start) >  (min_show_segment_length) ))
                  )
            {
                context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                context.state.commercial[context.state.commercial_count].length = frame_duration(context, context.state.commercial[context.state.commercial_count].end_frame, context.state.commercial[context.state.commercial_count].start_frame);
                context.state.commercial[context.state.commercial_count].end_block = i;
                context.state.cblock[i].iscommercial = true;
                i++;
            }
        }
        else
            context.state.cblock[i].iscommercial = false;
        i++;
    }
#endif


    if (!(context.settings.disable_heuristics & (1 << (5 - 1))))
    {

        if (context.settings.delete_block_after_commercial > 0)
        {
            for (k = context.state.commercial_count; k >= 0; k--)
            {
                i = context.state.commercial[k].end_block + 1;
                if (i < context.state.block_count && context.state.cblock[i].length < context.settings.delete_block_after_commercial &&
                        context.state.cblock[i].score < threshold)
                {
                    Debug(context, 3, context.translator.format("cutlists_h5_delete_after_commercial", i));
                    context.state.commercial[k].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                    context.state.commercial[k].length = frame_duration(context, context.state.commercial[k].end_frame, context.state.commercial[k].start_frame);
                    context.state.commercial[k].end_block = i;
                    context.state.cblock[i].iscommercial = true;
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
                    context.state.cblock[i].score = 99.99;
                    context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
                }
            }
        }

        if (context.state.commercial_count > -1 &&
                context.state.commercial[context.state.commercial_count].end_block < context.state.block_count - 1 &&
                frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[context.state.commercial[context.state.commercial_count].end_block].f_end) < context.settings.min_show_segment_length / 2.0 )
        {
            context.state.commercial[context.state.commercial_count].end_block = context.state.block_count-1;
            context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[context.state.block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].length = frame_duration(context, context.state.commercial[context.state.commercial_count].end_frame, context.state.commercial[context.state.commercial_count].start_frame);
            Debug(context, 3, context.translator.format("cutlists_h5_delete_after_last",
                context.state.block_count-1,
                static_cast<int>(context.state.cblock[context.state.block_count-1].length)));
            context.state.cblock[context.state.block_count-1].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
            context.state.cblock[context.state.block_count-1].score = 99.99;
            context.state.cblock[context.state.block_count-1].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
        }

        if (context.state.commercial_count > -1 &&
                context.state.commercial[0].start_block == 1 &&
                frame_time(context, context.state.cblock[0].f_end) < context.settings.min_commercialbreak)
        {
            context.state.commercial[0].start_block = 0;
            context.state.commercial[0].start_frame = context.state.cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[0].length = frame_duration(context, context.state.commercial[0].end_frame,	context.state.commercial[0].start_frame);
            Debug(context, 3, context.translator.format("cutlists_h5_delete_before_first", 0,
                static_cast<int>(context.state.cblock[0].length)));
            context.state.cblock[0].score = 99.99;
            context.state.cblock[0].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
            context.state.cblock[0].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);

        }

    }


    Debug(context, 2, context.translator.text("cutlists_initial_list"));
    for (i = 0; i <= context.state.commercial_count; i++)
    {
        Debug(context,
            2,
            "%2i) %6i\t%6i\t%s\n",
            i,
            context.state.commercial[i].start_frame,
            context.state.commercial[i].end_frame,
            dblSecondsToStrMinutes(context.state.commercial[i].length).c_str()
        );
    }

#if 1




    if (!(context.settings.disable_heuristics & (1 << (6 - 1))))
    {

        // Delete too long/short commercials
        for (k = context.state.commercial_count; k >= 0; k--)
        {
            if ( (frame_time(context, context.state.commercial[k].start_frame) > 1.0   || context.state.commercial[k].length < 10.2 /* Sage bug fix */ )
                    &&		// Do not delete too short first or last commercial
                    ((context.state.commercial[k].length > context.settings.max_commercialbreak && k != 0 && k != context.state.commercial_count) ||
                     (context.state.commercial[k].length < context.settings.min_commercialbreak)) &&
                    frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) > context.settings.min_commercial_break_at_start_or_end  &&
                    frame_time(context, context.state.commercial[k].end_frame) > context.settings.min_commercial_break_at_start_or_end )
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(context, 3, context.translator.format("cutlists_h6_delete_length", i));
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                    context.state.cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                }
                comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
                deleted = true;
            }
        }
#ifdef NOTDEF
// keep first seconds
        if (always_keep_first_seconds && context.state.commercial_count >= 0)
        {
            k = 0;
            if ( frame_time(context, context.state.commercial[k].end_frame) < always_keep_first_seconds)
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the first %d seconds should always be kept.\n",
                          i, always_keep_first_seconds);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                    context.state.cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                }
                comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
                deleted = true;
            }
        }
        if (always_keep_last_seconds && context.state.commercial_count >= 0)
        {
            k = context.state.commercial_count;
            if (frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) < always_keep_last_seconds)
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the last %d seconds should always be kept.\n",
                          i, always_keep_last_seconds);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                    context.state.cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                }
                comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
                deleted = true;
            }
        }
#endif

        /*
                // Delete too short first commercial
                k = 0;
                if (commercial_count >= 0 && commercial[k].start_frame < fps &&
                    commercial[k].length < min_commercial_break_at_start_or_end) {
                    for (i = commercial[k].start_block; i <= commercial[k].end_block; i++) {
                        Debug(3, "H6 Deleting block %i because it is part of a too short commercial at the start of the recording.\n",
                            i);
                        cblock[i].score = 0;
                        cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                        cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                    }
                    for (i = k; i < commercial_count; i++) {
                        commercial[i] = commercial[i + 1];
                    }
                    commercial_count--;
                    deleted = true;
                }
                // Delete too short last commercial
                k = commercial_count;
                if (commercial_count >= 0 && (cblock[block_count-1].f_end - commercial[k].end_frame) < fps &&
                    commercial[k].length < min_commercial_break_at_start_or_end) {
                    for (i = commercial[k].start_block; i <= commercial[k].end_block; i++) {
                        Debug(3, "H6 Deleting block %i because it is part of a too short commercial at the end of the recording.\n",
                            i);
                        cblock[i].score = 0;
                        cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                        cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                    }
                    for (i = k; i < commercial_count; i++) {
                        commercial[i] = commercial[i + 1];
                    }
                    commercial_count--;
                    deleted = true;
                }
        */
        /*
            // Delete too short shows
            for (k = commercial_count-1; k >= 0; k--) {
                if ( commercial[k+1].start_frame - commercial[k].end_frame < min_show_segment_length / 2.5 * fps ||
                     (commercial[k].end_frame > after_start &&
                      commercial[k].end_frame < before_end &&
                      commercial[k+1].start_frame - commercial[k].end_frame < min_show_segment_length  * fps)
                    ) {
                    for (i = commercial[k].end_block+1; i < commercial[k+1].start_block; i++) {
                        cblock[i].score = 99.99;
                        cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                        cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_6);
                    }
                    commercial[k].end_block = commercial[k+1].end_block;
                    commercial[k].end_frame = commercial[k+1].end_frame;
                    commercial[k].length = (commercial[k].end_frame - commercial[k].start_frame) / fps;

                    for (i = k+1; i < commercial_count; i++) {
                            commercial[i] = commercial[i + 1];
                    }
                    commercial_count--;
                    deleted = true;
                }
            }
        */

    }
    if (context.settings.delete_show_after_last_commercial &&
            context.state.commercial_count > -1 &&
            //	( commercial[commercial_count].end_block == block_count - 2 || commercial[commercial_count].end_block == block_count - 3) &&
            ((context.settings.delete_show_after_last_commercial == 1 && context.state.cblock[context.state.commercial[context.state.commercial_count].start_block].f_end > context.state.before_end) ||
             (context.settings.delete_show_after_last_commercial > frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[context.state.commercial[context.state.commercial_count].start_block].f_start)) )

            &&
            context.state.commercial[context.state.commercial_count].end_block < context.state.block_count-1
       )
    {
        i = context.state.commercial[context.state.commercial_count].end_block + 1;
        context.state.commercial[context.state.commercial_count].end_block = context.state.block_count-1;
        context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[context.state.block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
        context.state.commercial[context.state.commercial_count].length = frame_duration(context, context.state.commercial[context.state.commercial_count].end_frame,	context.state.commercial[context.state.commercial_count].start_frame);
        while (i < context.state.block_count)
        {
            Debug(context, 3, context.translator.format("cutlists_h5_delete_after_last_all", i,
                static_cast<int>(context.state.cblock[i].length)));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
            context.state.cblock[i].score = 99.99;
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
            i++;
        }
    }



    if (context.settings.delete_show_before_first_commercial &&
            context.state.commercial_count > -1 &&
            context.state.commercial[0].start_block == 1 &&
            ((context.settings.delete_show_before_first_commercial == 1 && context.state.cblock[context.state.commercial[0].end_block].f_end < context.state.after_start) ||
             (context.settings.delete_show_before_first_commercial > frame_time(context, context.state.cblock[context.state.commercial[0].end_block].f_end)))
       )
    {
        context.state.commercial[0].start_block = 0;
        context.state.commercial[0].start_frame = context.state.cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
        context.state.commercial[0].length = frame_duration(context, context.state.commercial[0].end_frame, context.state.commercial[0].start_frame);
        Debug(context, 3, context.translator.format("cutlists_h5_delete_before_first_all", 0,
            static_cast<int>(context.state.cblock[0].length)));
        context.state.cblock[0].score = 99.99;
        context.state.cblock[0].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);
        context.state.cblock[0].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_5);

    }

// keep first seconds
    if (context.settings.always_keep_first_seconds && context.state.commercial_count >= 0)
    {
        k = 0;
        while (context.state.commercial_count >= 0 && frame_time(context, context.state.commercial[k].end_frame) < context.settings.always_keep_first_seconds)
        {
            Debug(context, 3, context.translator.format("cutlists_keep_first_delete", k,
                context.settings.always_keep_first_seconds));
            comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
            deleted = true;
        }
        if (context.state.commercial_count >= 0 && frame_time(context, context.state.commercial[k].start_frame ) < context.settings.always_keep_first_seconds)
        {
            Debug(context, 3, context.translator.format("cutlists_keep_first_shorten", k,
                context.settings.always_keep_first_seconds));
            while (frame_time(context, context.state.commercial[k].start_frame ) < context.settings.always_keep_first_seconds && context.state.commercial[k].start_frame < context.settings.always_keep_first_seconds * context.settings.fps)
                context.state.commercial[k].start_frame++;
        }
    }
    if (context.settings.always_keep_last_seconds && context.state.commercial_count >= 0)
    {
        k = context.state.commercial_count;
        while (context.state.commercial_count >= 0 && frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) < context.settings.always_keep_last_seconds)
        {
            Debug(context, 3, context.translator.format("cutlists_keep_last_delete", k,
                context.settings.always_keep_last_seconds));
            comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
            k = context.state.commercial_count;
            deleted = true;
        }
        if (context.state.commercial_count >= 0 && frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].end_frame) < context.settings.always_keep_last_seconds)
        {
            Debug(context, 3, context.translator.format("cutlists_keep_last_shorten", k,
                context.settings.always_keep_last_seconds));
            while (frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].end_frame) < context.settings.always_keep_last_seconds && (context.state.cblock[context.state.block_count-1].f_end - context.state.commercial[k].end_frame) < context.settings.fps * context.settings.always_keep_last_seconds)
                context.state.commercial[k].end_frame--;
        }
    }



    if (deleted)
        Debug(context, 1, context.translator.text("cutlists_final_list"));
    else
        Debug(context, 1, context.translator.text("cutlists_no_change"));
#endif


    // Apply padding
    for (i = 0; i <= context.state.commercial_count; i++)
    {
        context.state.commercial[i].start_frame += context.settings.padding*context.settings.fps - context.settings.remove_before*context.settings.fps;
        context.state.commercial[i].end_frame -= context.settings.padding*context.settings.fps - context.settings.remove_after*context.settings.fps;
        if (context.state.commercial[i].end_frame > context.state.frame_count)
            context.state.commercial[i].end_frame = context.state.frame_count;
        context.state.commercial[i].length += -2*context.settings.padding + context.settings.remove_before + context.settings.remove_after;
    }



    comlength = 0.;
    for (i = 0; i < context.state.commercial_count; i++)
    {
        comlength += context.state.commercial[i].length;
    }
//	Debug(1, "Total commercial length found: %s\n",	dblSecondsToStrMinutes(comlength));

    prev = -1;
    for (i = 0; i <= context.state.commercial_count; i++)
    {
//		if ((commercial[i].length >= min_commercialbreak) && (commercial[i].length <= max_commercialbreak))
        {
            foundCommercials = true;
            if (deleted)
                Debug(context,
                    1,
                    "%i - start: %6i\tend: %6i\t[%6i:%6i]\tlength: %s\n",
                    i + 1,
                    context.state.commercial[i].start_frame,
                    context.state.commercial[i].end_frame,
                    context.state.commercial[i].start_block,
                    context.state.commercial[i].end_block,
                    dblSecondsToStrMinutes(context.state.commercial[i].length).c_str()
                );
            OutputCommercialBlock(context, i, prev, context.state.commercial[i].start_frame, context.state.commercial[i].end_frame, (context.state.commercial[i].end_frame < context.state.frame_count-2 ? false : true));
            prev = context.state.commercial[i].end_frame;
        }
    }

    if (context.state.commercial_count < 0 ||
        context.state.commercial[context.state.commercial_count].end_frame < context.state.frame_count-2)
        OutputCommercialBlock(context, context.state.commercial_count+1, prev, context.state.frame_count-2, context.state.frame_count-1, true);

    WriteXmlOutputFiles(context);
    WriteFfmpegSidecarFiles(context);
    WriteFrameScriptFiles(context);
    WritePlayerExportFiles(context);
    WriteLegacyEditorFiles(context);
    WriteLegacyCutlistFiles(context);

    if (context.state.reffer_count == -1) {
        std::vector<Legacy_reffer_entry> reference;
        reference.reserve(context.state.commercial.size());
        for (i = 0; i <= context.state.commercial_count; i++)
        {
            reference.push_back({context.state.commercial[i].start_frame, context.state.commercial[i].end_frame});
        }
        context.state.reffer = std::move(reference);
        context.state.reffer_count = context.state.commercial_count;
    }

    InputReffer(context, ".ref", false);

    if (context.settings.output_tuning)
    {
        context.state.filename = std::string(context.state.workbasename) + ".tun";
        context.state.tuning_file=open_checked_file(context.state.filename,"w");
        comskip::output::checked_fprintf(*context.state.tuning_file,context.state.filename,"max_volume=%6i\n", context.state.min_volume+200);
        comskip::output::checked_fprintf(*context.state.tuning_file,context.state.filename,"max_avg_brightness=%6i\n", context.state.min_brightness_found+5);
        comskip::output::checked_fprintf(*context.state.tuning_file,context.state.filename,"max_commercialbreak=%6i\n", context.state.max_logo_gap+10);
        comskip::output::checked_fprintf(*context.state.tuning_file,context.state.filename,"shrink_logo=%.2f\n", context.state.logo_overshoot);
        comskip::output::checked_fprintf(*context.state.tuning_file,context.state.filename,"min_show_segment_length=%6i\n", context.state.max_nonlogo_block_length+10);
        comskip::output::checked_fprintf(*context.state.tuning_file,context.state.filename,"logo_threshold=%.3f\n", context.state.logo_quality);
        comskip::output::checked_close(context.state.tuning_file,context.state.filename);
    }




    if (context.settings.verbose)
    {
        Debug(context, 1, context.translator.format("cutlists_statistics",
            std::format("{:.4f}", context.state.logoPercentage),
            comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo)
                ? (context.state.reverseLogoLogic ? "(Reversed Logo Logic)" : "") : "Logo disabled",
            std::format("{:6}", context.state.maxi_volume), std::format("{:6}", context.state.avg_volume),
            std::format("{:6}", context.settings.max_volume), std::format("{:6}", context.settings.max_silence),
            std::format("{:6}", context.state.min_volume), std::format("{:6}", context.state.avg_silence),
            std::format("{:6}", context.settings.max_avg_brightness), std::format("{:6}", context.state.min_brightness_found),
            std::format("{:6}", context.state.min_hasBright), std::format("{:6}", context.state.min_dimCount),
            std::format("{:6}", context.state.avg_brightness), std::format("{:6}", context.settings.non_uniformity),
            std::format("{:6}", context.state.avg_uniform), std::format("{:6}", context.state.max_logo_gap),
            std::format("{:.4f}", context.state.logo_quality), std::format("{:.2f}", context.state.logo_overshoot),
            std::format("{:6}", context.state.max_nonlogo_block_length), std::format("{:.4f}", context.state.dominant_ar),
            std::format("{:.4f}", threshold), std::format("{:2.3f}", context.settings.fps),
            std::format("{:2.3f}", context.state.avg_fps)));

        Debug(context, 1, context.translator.format("cutlists_total_commercial_length",
            dblSecondsToStrMinutes(comlength)));
        Debug(context, 1, context.translator.text("cutlists_cut_codes"));
        Debug(context, 1, context.translator.text("cutlists_weighted_heading"));
        Debug(context,
            1,
            "  #     sbf  bs  be     fs     fe        ts        te       len     sc   scr cmb   ar                   cut    bri logo   vol sil   corr stdev   cc\n"
        );

//		if (output_training) {
//			fprintf(training_file, TRAINING_LAYOUT,
//				"0", 0, 0, 0, 0,0,0, 0, 0, 0, 0);
//		}




        for (i = 0; i < context.state.block_count; i++)
        {
            /*
                        cs[5] = (cblock[i].cause & 16 ? 'b' : ' ');
                        cs[4] = (cblock[i].cause & 8  ? 'u' : ' ');
                        cs[3] = (cblock[i].cause & 32 ? 'a' : ' ');
                        cs[2] = (cblock[i].cause & 4  ? 's' : ' ');
                        cs[1] = (cblock[i].cause & 1  ? 'l' : ' ');
                        cs[0] = (cblock[i].cause & 2  ? 'c' : ' ');
                        cs[6] = 0;
            */

            Debug(context,
                1,
                "%3i:%c%c %4i %3i %3i %6i %6i %8.2fs %8.2fs %8.2fs %6.2f %5.2f %3i %4.2f %s %4i%c %4.2f %4i%c %2i%c %6.3f %5i %-10s",
                i,
                CheckFramesForCommercial(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                CheckFramesForReffer(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                context.state.cblock[i].bframe_count,
                context.state.cblock[i].b_head,
                context.state.cblock[i].b_tail,
                context.state.cblock[i].f_start,
                context.state.cblock[i].f_end,
                get_frame_pts(context, context.state.cblock[i].f_start),
                get_frame_pts(context, context.state.cblock[i].f_end),
                context.state.cblock[i].length,
                context.state.cblock[i].score,
//				cblock[i].schange_count,
                context.state.cblock[i].schange_rate,
                context.state.cblock[i].combined_count,
                context.state.cblock[i].ar_ratio,
                CauseString(context, context.state.cblock[i].cause),
                context.state.cblock[i].brightness,
                CompareLetter(context, context.state.cblock[i].brightness,context.state.avg_brightness,i),
                context.state.cblock[i].logo,
                context.state.cblock[i].volume,
                CompareLetter(context, context.state.cblock[i].volume,context.state.avg_volume,i),
                context.state.cblock[i].silence,
                CompareLetter(context, context.state.cblock[i].silence,context.state.avg_silence,i),
                0.0 /*cblock[i].correlation */ ,
                context.state.cblock[i].stdev,
                CCTypeText(context, context.state.cblock[i].cc_type).c_str()
            );
            if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo))
            {
//				if (CheckFramesForLogo(cblock[i].f_start, cblock[i].f_end)) {
//					Debug(1, "\tLogo Present\n");
//				} else {
                Debug(context, 1, "\n");
//				}
            }
            else
            {
                Debug(context, 1, "\n");
            }
        }

        OutputAspect(context);
        OutputTraining(context);



//		if (output_training) {
//			fprintf(training_file, TRAINING_LAYOUT,
//				"0", 0, 0, 100, 0,0,0, 0, 0, 0, 0);
//		}

    }

//	OutputDebugWindow(false,0);
    return (foundCommercials);
}

void OutputStrict(RecordingContext& context, double len, double delta, double tol)
{
//return;
    if (context.settings.output_training && !context.state.training_file.get())
    {
        context.state.training_file=open_checked_file("strict.csv","a+");
//		fprintf(training_file, "// score, length, fraction, position,combined, ar error, logo, strict \n");
    }
    if (context.state.training_file.get())
        comskip::output::checked_fprintf(*context.state.training_file,"strict.csv","%+f,%+f,%+f,%s\n", len,delta, tol, comskip::output::csv_field(context.state.inbasename).c_str());
}




void OutputTraining(RecordingContext& context)
{
    int i;
//	return;
    if (!context.settings.output_training)
        return;
    if (context.state.training_file)
        comskip::output::checked_close(context.state.training_file,"strict.csv");
    context.state.training_file=open_checked_file("comskip.csv","a+");

    comskip::output::checked_fprintf(*context.state.training_file,"comskip.csv", "block, cm,rf, score, length, start, end, fromend ar, logo, cause, less, more\n");

    for (i = 0; i < context.state.block_count; i++)
    {
        if (context.settings.output_training)
        {
            comskip::output::checked_fprintf(*context.state.training_file,"comskip.csv",training_layout.data(),
                    i,
                    CheckFramesForCommercial(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                    CheckFramesForReffer(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                    context.state.cblock[i].score,
                    context.state.cblock[i].length,
                    frame_time(context, context.state.cblock[i].f_start),
                    frame_time(context, context.state.cblock[i].f_end),
                    frame_duration(context, context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[i].f_end),
                    context.state.cblock[i].ar_ratio,
                    context.state.cblock[i].logo,
                    CauseString(context, context.state.cblock[i].cause),
                    CauseString(context, context.state.cblock[i].less),
                    CauseString(context, context.state.cblock[i].more),
                    comskip::output::csv_field(context.state.inbasename).c_str());

        }
    }
    comskip::output::checked_close(context.state.training_file,"comskip.csv");
}
