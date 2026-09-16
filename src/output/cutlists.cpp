#include "platform/utf8_paths.h"
#include "exit_requested.h"
#include "cutlist_exports.h"
#include "checked_format.h"
#include "xml_output_adapter.h"
#include "ffmpeg_sidecar_adapter.h"
#include "frame_script_adapter.h"
#include "player_export_adapter.h"
#include "legacy_editor_adapter.h"
#include "csv_field.h"
#include "edl.h"
#include <sstream>
#include <vector>
#include "legacy_detection.h"

namespace {
void append_edl_record(RecordingContext& context, FILE* destination, long start, long end,
                       comskip::output::EdlVariant variant)
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
        throw std::ios_base::failure("Failed writing commercial EDL output");
}
}


void OpenOutputFiles(RecordingContext& context)
{

    if (context.settings.output_default)
    {
        context.state.out_file.reset(myfopen(context.state.out_filename.c_str(), "w"));
        if (!context.state.out_file.get())
        {
            sleep_for_ms(50L);
            context.state.out_file.reset(myfopen(context.state.out_filename.c_str(), "w"));
            if (!context.state.out_file.get())
            {
                Debug(context, 0, "%s", context.translator.format("cutlists_write_failed", context.state.out_filename.c_str()).c_str());
                comskip::request_exit(103);
            }
        }
        fprintf(context.state.out_file.get(), "FILE PROCESSING COMPLETE %6li FRAMES AT %5i\n-------------------\n",F2F(context.state.frame_count-1), (int)(context.settings.fps*100));
        context.state.out_file.reset();
    }

    if (context.settings.output_chapters)
    {
        context.state.filename = std::string(context.state.outbasename) + ".chap";
        context.state.chapters_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (!context.state.chapters_file.get())
        {
            sleep_for_ms(50L);
            context.state.chapters_file.reset(myfopen(context.state.filename.c_str(), "w"));
            if (!context.state.chapters_file.get())
            {
                Debug(context, 0, "%s", context.translator.format("cutlists_write_failed", context.state.filename.c_str()).c_str());
                comskip::request_exit(103);
            }
        }
        fprintf(context.state.chapters_file.get(), "FILE PROCESSING COMPLETE %6li FRAMES AT %5i\n-------------------\n",context.state.frame_count-1, (int)(context.settings.fps*100));
    }

    if (context.settings.output_incommercial)
    {
        context.state.filename = std::string(context.state.workbasename) + ".incommercial";
        context.state.incommercial_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (!context.state.incommercial_file.get())
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
        fprintf(context.state.incommercial_file.get(), "0\n");
        context.state.incommercial_file.reset();
    }




    if (context.settings.output_edl)
    {
        context.state.filename = std::string(context.state.outbasename) + ".edl";
        context.state.edl_file.reset(myfopen(context.state.filename.c_str(), "wb"));
        if (!context.state.edl_file.get())
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_edl = true;
        }
    }

/*
    if (output_live)
    {
        filename = std::string(outbasename) + ".live";
        live_file = myfopen(filename, "wb");
        if (!live_file)
        {
            fputs(context.translator.format("create_failed", strerror(errno), filename).c_str(), stderr);
            comskip::request_exit(6);
        }
        else
        {
            output_live = true;
        }
    }
*/
    if (context.settings.output_edlp)
    {
        context.state.filename = std::string(context.state.outbasename) + ".edlp";
        context.state.edlp_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (!context.state.edlp_file.get())
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
        else
        {
            context.settings.output_edlp = true;
        }
    }


    if (context.settings.output_womble)
    {
        context.state.filename = std::string(context.state.outbasename) + ".wme";
        context.state.womble_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (context.state.womble_file.get())
        {
//			fclose(womble_file);
            context.settings.output_womble = true;
        }
        else
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_mls)
    {
        context.state.filename = std::string(context.state.outbasename) + ".mls";
        context.state.mls_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (context.state.mls_file.get())
        {
//			fclose(mls_file);
            context.settings.output_mls = true;
//[BookmarkList]
//PathName= C:\VidTst\Will - Grace - Secrets - Lays.mpg
//VideoStreamID= 224
//Format= frame
//Count= 19

        }
        else
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_mpgtx)
    {
        context.state.filename = std::string(context.state.outbasename) + "_mpgtx.bat";
        context.state.mpgtx_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (context.state.mpgtx_file.get())
        {
//			fclose(mpgtx_file);
            context.settings.output_mpgtx = true;
            fprintf(context.state.mpgtx_file.get(), "mpgtx.exe -j -f -o \"%s%s\" \"%s\" ", context.state.mpegfilename.c_str(), ".clean", context.state.mpegfilename.c_str());
        }
        else
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
    }

    if (context.settings.output_dvrcut)
    {
        context.state.filename = std::string(context.state.outbasename) + "_dvrcut.bat";
        context.state.dvrcut_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (context.state.dvrcut_file.get())
        {
//			fclose(dvrcut_file);
            if (context.settings.dvrcut_options.c_str()[0] == 0)
                fprintf(context.state.dvrcut_file.get(), "dvrcut \"%%1\" \"%%2\" ");
            else
                fprintf(context.state.dvrcut_file.get(), context.settings.dvrcut_options.c_str(), context.state.inbasename.c_str(), context.state.inbasename.c_str(), context.state.inbasename.c_str()  );
        }
        else
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
    }


    if (context.settings.output_mpeg2schnitt)
    {
        context.state.filename = std::string(context.state.inbasename) + "_mpeg2schnitt.bat";
        context.state.mpeg2schnitt_file.reset(myfopen(context.state.filename.c_str(), "w"));
        if (context.state.mpeg2schnitt_file.get())
        {
//			fclose(mpeg2schnitt_file);
            context.settings.output_mpeg2schnitt = true;
// Mpeg2Schnitt.exe %1.m2v /R29.97 /o250 /i550 /o3210 /i4000 /S /E /Z %2.m2v
            if (context.settings.mpeg2schnitt_options.c_str()[0] == 0)
                fprintf(context.state.mpeg2schnitt_file.get(), "mpeg2schnitt.exe /S /E /R%5.2f  /Z \"%s\" \"%s\" ", context.settings.fps, "%2", "%1");
            else
                fprintf(context.state.mpeg2schnitt_file.get(), "%s ", context.settings.mpeg2schnitt_options.c_str());
        }
        else
        {
            fputs(context.translator.format("create_failed", strerror(errno), context.state.filename).c_str(), stderr);
            comskip::request_exit(6);
        }
    }
}

#define CLOSEOUTFILE(F) do { if (last) (F).reset(); } while (false)

void OutputCommercialBlock(RecordingContext& context, int i, long prev, long start, long end, bool last)
{
    int s_start, s_end;
    int count;
    double minutes = F2T(context.state.frame_count)/60;

/*
    // Convert from frame array index to (timecode / fps) for external output
    if (prev > 0)
        prev = F2F(prev);
    if (start > 0 && start <= frame_count)
        start = F2F(start);
    if (end > 0 && end <= frame_count)
        end = F2F(end);

    start = max(start,0);
    end = max(end,0);
*/

    s_start = start;
    s_end = end;

    if (context.settings.sage_minute_bug)
    {
        s_start = (int)(start * (((int)( minutes+0.5))/minutes));
        s_end = (int)(end * (((int)(minutes+0.5))/minutes));
    }
    if (context.settings.output_default && prev < start /*&& !last */)
    {
        context.state.out_file.reset(myfopen(context.state.out_filename.c_str(), "a+"));
        if (context.state.out_file.get())
        {
            fprintf(context.state.out_file.get(), "%li\t%li\n", F2F(context.settings.sage_framenumber_bug?s_start/2:s_start), F2F(context.settings.sage_framenumber_bug?s_end/2:s_end));
            context.state.out_file.reset();
        }
        else  		// If the file can't be opened for writting, wait half a second and try again
        {
            sleep_for_ms(50L);
            context.state.out_file.reset(myfopen(context.state.out_filename.c_str(), "a+"));
            if (context.state.out_file.get())
            {
                fprintf(context.state.out_file.get(), "%li\t%li\n", F2F(context.settings.sage_framenumber_bug?s_start/2:s_start), F2F(context.settings.sage_framenumber_bug?s_end/2:s_end));
                context.state.out_file.reset();
            }
            else  	// If the file still can't be opened for writting, give up and exit
            {
                Debug(context, 0, "%s", context.translator.format("cutlists_write_failed", context.state.out_filename.c_str()).c_str());
                comskip::request_exit(103);
            }
        }
    }
    //CLOSEOUTFILE(context.state.out_file);

    if (context.state.edl_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        append_edl_record(context, context.state.edl_file.get(), start < 5 ? 0 : start, end, comskip::output::EdlVariant::standard);
    }
    CLOSEOUTFILE(context.state.edl_file);

    if (context.state.live_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        append_edl_record(context, context.state.live_file.get(), start < 5 ? 0 : start, end, comskip::output::EdlVariant::standard);
    }
    CLOSEOUTFILE(context.state.live_file);

    if (context.state.edlp_file.get() && prev < start /* &&!last */ && end - start > 2)
    {
        append_edl_record(context, context.state.edlp_file.get(), start < 5 ? 0 : start, end, comskip::output::EdlVariant::plus);
    }
    CLOSEOUTFILE(context.state.edlp_file);

    if (context.state.womble_file.get())
    {
// CLIPLIST: #1 show
// CLIP: morse.mpg
// 6 0 9963
        if (!last)
        {
            if (start - prev > context.settings.fps)
            {
                fprintf(context.state.womble_file.get(), "CLIPLIST: #%i show\nCLIP: %s\n6 %li %li\n", i+1, context.state.mpegfilename.c_str(),F2F(prev+1), F2F(start) - F2F(prev));
            }
// CLIPLIST: #2 commercial
// CLIP: morse.mpg
// 6 9963 5196

            fprintf(context.state.womble_file.get(), "CLIPLIST: #%i commercial\nCLIP: %s\n6 %li %li\n", i+1, context.state.mpegfilename.c_str(), F2F(start), F2F(end) - F2F(start));
        }
        else
        {
            if (end - prev > 0)
                fprintf(context.state.womble_file.get(), "CLIPLIST: #%i show\nCLIP: %s\n6 %li %li\n", i+1, context.state.mpegfilename.c_str(), F2F(prev+1), F2F(end) - F2F(prev));
        }
    }
    CLOSEOUTFILE(context.state.womble_file);

    if (context.state.mls_file.get())
    {
        if (i == 0)
        {
            count = (context.state.commercial_count+1)*2+1;
//            if (commercial[commercial_count].end_frame < frame_count-2)
//                count += 2;
            if (start < context.settings.fps)
                count -= 1;
            fprintf(context.state.mls_file.get(), "[BookmarkList]\nPathName= %s\nVideoStreamID= 0\nFormat= frame\nCount= %d\n", context.state.mpegfilename.c_str(), count);
            if (start >= context.settings.fps)
                fprintf(context.state.mls_file.get(), "%11i 1\n", 0);
        }
        else
            fprintf(context.state.mls_file.get(), "%11li 1\n", F2F(prev));
        if (!last)
            fprintf(context.state.mls_file.get(), "%11li 0\n", F2F(start));
        else if (start < end - 5) {
            fprintf(context.state.mls_file.get(), "%11li 0\n", F2F(start));
            fprintf(context.state.mls_file.get(), "%11li 1\n", F2F(end));
        }

    }
    CLOSEOUTFILE(context.state.mls_file);

    if (context.state.mpgtx_file.get())
    {
        if (!last)
        {
            if (start - prev > 0)
            {
                fprintf(context.state.mpgtx_file.get(), "[%s-",	(prev < context.settings.fps ? "":intSecondsToStrMinutes(context,  (int)get_frame_pts(context, prev))));
                fprintf(context.state.mpgtx_file.get(), "%s] ", intSecondsToStrMinutes(context,  (int)get_frame_pts(context, start)));
            }
        }
        else
        {
            if (end - prev > 0)
                fprintf(context.state.mpgtx_file.get(), "[%s-]",	intSecondsToStrMinutes(context,  (int)get_frame_pts(context, prev+1)));
            fprintf(context.state.mpgtx_file.get(), "\n");
        }
    }
    CLOSEOUTFILE(context.state.mpgtx_file);

    if (context.state.dvrcut_file.get())
    {
        if (start - prev > (int)context.settings.fps /* && start > 2*fps */)
        {
            fprintf(context.state.dvrcut_file.get(), "%s ",	intSecondsToStrMinutes(context,  (int)get_frame_pts(context, prev)));
            fprintf(context.state.dvrcut_file.get(), "%s ", intSecondsToStrMinutes(context,  (int)get_frame_pts(context, start)));
        }
        if (last)
        {
            fprintf(context.state.dvrcut_file.get(), "\n");
        }
    }
    CLOSEOUTFILE(context.state.dvrcut_file);

    if (context.state.mpeg2schnitt_file.get())
    {
        if (end - start > 1)
        {
            fprintf(context.state.mpeg2schnitt_file.get(), "/o%ld ",	F2F(start));
            fprintf(context.state.mpeg2schnitt_file.get(), "/i%ld ", F2F(end));
        }
        if (last)
        {
            fprintf(context.state.mpeg2schnitt_file.get(), "\n");
        }
    }
    CLOSEOUTFILE(context.state.mpeg2schnitt_file);
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
                Legacy_commercial_entry{block.f_start, block.f_end, i, i, F2L(block.f_end, block.f_start)});
        else {
            auto& interval = intervals.back();
            interval.end_frame = block.f_end;
            interval.end_block = i;
            interval.length = F2L(interval.end_frame, interval.start_frame);
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


    Debug(context, 1, "Threshold used - %.4f", threshold);
    threshold = ceil(threshold * 100) / 100.0;
    Debug(context, 1, "\tAfter rounding - %.4f\n", threshold);

    BuildCommercial(context);

#ifdef undef
    context.state.commercial_count = -1;
    i = 0;
    while (i < context.state.block_count)
    {
        if (context.state.cblock[i].score > threshold
//			&&
//			( cblock[i].score >= 100 ||
//			!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > (min_show_segment_length) ))
           )
        {
            context.state.commercial_count++;
            context.state.commercial[context.state.commercial_count].start_frame = context.state.cblock[i].f_start/*+ (cblock[i].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame,	context.state.commercial[context.state.commercial_count].start_frame);
            context.state.commercial[context.state.commercial_count].start_block = i;
            context.state.commercial[context.state.commercial_count].end_block = i;
            context.state.cblock[i].iscommercial = true;
            i++;
            while (i < context.state.block_count && context.state.cblock[i].score > threshold
//				&&
//				( cblock[i].score >= 100 ||
//				!((commDetectMethod & LOGO) && cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) >  (min_show_segment_length) ))
                  )
            {
                context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame, context.state.commercial[context.state.commercial_count].start_frame);
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
                    Debug(context, 3, "H5 Deleting cblock %i because it is short and comes after a commercial.\n",
                          i);
                    context.state.commercial[k].end_frame = context.state.cblock[i].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
                    context.state.commercial[k].length = F2L(context.state.commercial[k].end_frame, context.state.commercial[k].start_frame);
                    context.state.commercial[k].end_block = i;
                    context.state.cblock[i].iscommercial = true;
                    context.state.cblock[i].cause |= C_H5;
                    context.state.cblock[i].score = 99.99;
                    context.state.cblock[i].more |= C_H5;
                }
            }
        }

        if (context.state.commercial_count > -1 &&
                context.state.commercial[context.state.commercial_count].end_block < context.state.block_count - 1 &&
                F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[context.state.commercial[context.state.commercial_count].end_block].f_end) < context.settings.min_show_segment_length / 2.0 )
        {
            context.state.commercial[context.state.commercial_count].end_block = context.state.block_count-1;
            context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[context.state.block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame, context.state.commercial[context.state.commercial_count].start_frame);
            Debug(context, 3, "H5 Deleting cblock %i of %i seconds because it comes after the last commercial and its too short.\n",
                  context.state.block_count-1, (int)context.state.cblock[context.state.block_count-1].length);
            context.state.cblock[context.state.block_count-1].cause |= C_H5;
            context.state.cblock[context.state.block_count-1].score = 99.99;
            context.state.cblock[context.state.block_count-1].more |= C_H5;
        }

        if (context.state.commercial_count > -1 &&
                context.state.commercial[0].start_block == 1 &&
                F2T(context.state.cblock[0].f_end) < context.settings.min_commercialbreak)
        {
            context.state.commercial[0].start_block = 0;
            context.state.commercial[0].start_frame = context.state.cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
            context.state.commercial[0].length = F2L(context.state.commercial[0].end_frame,	context.state.commercial[0].start_frame);
            Debug(context, 3, "H5 Deleting cblock %i of %i seconds because its too short and before first commercial.\n",
                  0, (int)context.state.cblock[0].length);
            context.state.cblock[0].score = 99.99;
            context.state.cblock[0].cause |= C_H5;
            context.state.cblock[0].more |= C_H5;

        }

    }


    Debug(context, 2, "\n\n\t---------------------\n\tInitial Commercial List\n\t---------------------\n");
    for (i = 0; i <= context.state.commercial_count; i++)
    {
        Debug(context,
            2,
            "%2i) %6i\t%6i\t%s\n",
            i,
            context.state.commercial[i].start_frame,
            context.state.commercial[i].end_frame,
            dblSecondsToStrMinutes(context, context.state.commercial[i].length)
        );
    }

#if 1




    if (!(context.settings.disable_heuristics & (1 << (6 - 1))))
    {

        // Delete too long/short commercials
        for (k = context.state.commercial_count; k >= 0; k--)
        {
            if ( (F2T(context.state.commercial[k].start_frame) > 1.0   || context.state.commercial[k].length < 10.2 /* Sage bug fix */ )
                    &&		// Do not delete too short first or last commercial
                    ((context.state.commercial[k].length > context.settings.max_commercialbreak && k != 0 && k != context.state.commercial_count) ||
                     (context.state.commercial[k].length < context.settings.min_commercialbreak)) &&
                    F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) > context.settings.min_commercial_break_at_start_or_end  &&
                    F2T(context.state.commercial[k].end_frame) > context.settings.min_commercial_break_at_start_or_end )
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(context, 3, "H6 Deleting block %i because it is part of a too short or too long commercial.\n",
                          i);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= C_H6;
                    context.state.cblock[i].less |= C_H6;
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
            if ( F2T(context.state.commercial[k].end_frame) < always_keep_first_seconds)
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the first %d seconds should always be kept.\n",
                          i, always_keep_first_seconds);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= C_H6;
                    context.state.cblock[i].less |= C_H6;
                }
                comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
                deleted = true;
            }
        }
        if (always_keep_last_seconds && context.state.commercial_count >= 0)
        {
            k = context.state.commercial_count;
            if (F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) < always_keep_last_seconds)
            {
                for (i = context.state.commercial[k].start_block; i <= context.state.commercial[k].end_block; i++)
                {
                    Debug(3, "H6 Deleting block %i because the last %d seconds should always be kept.\n",
                          i, always_keep_last_seconds);
                    context.state.cblock[i].score = 0;
                    context.state.cblock[i].cause |= C_H6;
                    context.state.cblock[i].less |= C_H6;
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
                        cblock[i].cause |= C_H6;
                        cblock[i].less |= C_H6;
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
                        cblock[i].cause |= C_H6;
                        cblock[i].less |= C_H6;
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
                        cblock[i].cause |= C_H6;
                        cblock[i].less |= C_H6;
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
             (context.settings.delete_show_after_last_commercial > F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[context.state.commercial[context.state.commercial_count].start_block].f_start)) )

            &&
            context.state.commercial[context.state.commercial_count].end_block < context.state.block_count-1
       )
    {
        i = context.state.commercial[context.state.commercial_count].end_block + 1;
        context.state.commercial[context.state.commercial_count].end_block = context.state.block_count-1;
        context.state.commercial[context.state.commercial_count].end_frame = context.state.cblock[context.state.block_count-1].f_end/* + (cblock[i + 1].bframe_count / 2)*/;
        context.state.commercial[context.state.commercial_count].length = F2L(context.state.commercial[context.state.commercial_count].end_frame,	context.state.commercial[context.state.commercial_count].start_frame);
        while (i < context.state.block_count)
        {
            Debug(context, 3, "H5 Deleting cblock %i of %i seconds because it comes after the last commercial.\n",
                  i, (int)context.state.cblock[i].length );
            context.state.cblock[i].cause |= C_H5;
            context.state.cblock[i].score = 99.99;
            context.state.cblock[i].more |= C_H5;
            i++;
        }
    }



    if (context.settings.delete_show_before_first_commercial &&
            context.state.commercial_count > -1 &&
            context.state.commercial[0].start_block == 1 &&
            ((context.settings.delete_show_before_first_commercial == 1 && context.state.cblock[context.state.commercial[0].end_block].f_end < context.state.after_start) ||
             (context.settings.delete_show_before_first_commercial > F2T(context.state.cblock[context.state.commercial[0].end_block].f_end)))
       )
    {
        context.state.commercial[0].start_block = 0;
        context.state.commercial[0].start_frame = context.state.cblock[0].f_start/* + (cblock[i + 1].bframe_count / 2)*/;
        context.state.commercial[0].length = F2L(context.state.commercial[0].end_frame, context.state.commercial[0].start_frame);
        Debug(context, 3, "H5 Deleting cblock %i of %i seconds because it comes before the first commercial.\n",
              0, (int)context.state.cblock[0].length);
        context.state.cblock[0].score = 99.99;
        context.state.cblock[0].cause |= C_H5;
        context.state.cblock[0].more |= C_H5;

    }

// keep first seconds
    if (context.settings.always_keep_first_seconds && context.state.commercial_count >= 0)
    {
        k = 0;
        while (context.state.commercial_count >= 0 && F2T(context.state.commercial[k].end_frame) < context.settings.always_keep_first_seconds)
        {
            Debug(context, 3, "Deleting commercial block %i because the first %d seconds should always be kept.\n",
                  k, context.settings.always_keep_first_seconds);
            comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
            deleted = true;
        }
        if (context.state.commercial_count >= 0 && F2T(context.state.commercial[k].start_frame ) < context.settings.always_keep_first_seconds)
        {
            Debug(context, 3, "Shortening commercial block %i because the first %d seconds should always be kept.\n",
                  k, context.settings.always_keep_first_seconds);
            while (F2T(context.state.commercial[k].start_frame ) < context.settings.always_keep_first_seconds && context.state.commercial[k].start_frame < context.settings.always_keep_first_seconds * context.settings.fps)
                context.state.commercial[k].start_frame++;
        }
    }
    if (context.settings.always_keep_last_seconds && context.state.commercial_count >= 0)
    {
        k = context.state.commercial_count;
        while (context.state.commercial_count >= 0 && F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].start_frame) < context.settings.always_keep_last_seconds)
        {
            Debug(context, 3, "Deleting commercial block %i because the last %d seconds should always be kept.\n",
                  k, context.settings.always_keep_last_seconds);
            comskip::detection::erase_interval(context.state.commercial, context.state.commercial_count, k);
            k = context.state.commercial_count;
            deleted = true;
        }
        if (context.state.commercial_count >= 0 && F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].end_frame) < context.settings.always_keep_last_seconds)
        {
            Debug(context, 3, "Shortening commercial block %i because the last %d seconds should always be kept.\n",
                  k, context.settings.always_keep_last_seconds);
            while (F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.commercial[k].end_frame) < context.settings.always_keep_last_seconds && (context.state.cblock[context.state.block_count-1].f_end - context.state.commercial[k].end_frame) < context.settings.fps * context.settings.always_keep_last_seconds)
                context.state.commercial[k].end_frame--;
        }
    }



    if (deleted)
        Debug(context, 1, "\n\n\t---------------------\n\tFinal Commercial List\n\t---------------------\n");
    else
        Debug(context, 1, "No change\n");
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
                    dblSecondsToStrMinutes(context, context.state.commercial[i].length)
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

    if (context.settings.output_chapters)
    {
//		filename = std::string(outbasename) + ".chap";
//		chapters_file = myfopen(filename, "a+");
        if (context.state.chapters_file.get())
        {
            for (i = 0; i < context.state.block_count; i++)
            {
                fprintf(context.state.chapters_file.get(), "%ld\n", context.state.cblock[i].f_end);
            }
            context.state.chapters_file.reset();
        }
    }


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
        context.state.tuning_file.reset(myfopen(context.state.filename.c_str(), "w"));
        fprintf(context.state.tuning_file.get(),"max_volume=%6i\n", context.state.min_volume+200);
        fprintf(context.state.tuning_file.get(),"max_avg_brightness=%6i\n", context.state.min_brightness_found+5);
        fprintf(context.state.tuning_file.get(),"max_commercialbreak=%6i\n", context.state.max_logo_gap+10);
        fprintf(context.state.tuning_file.get(),"shrink_logo=%.2f\n", context.state.logo_overshoot);
        fprintf(context.state.tuning_file.get(),"min_show_segment_length=%6i\n", context.state.max_nonlogo_block_length+10);
        fprintf(context.state.tuning_file.get(),"logo_threshold=%.3f\n", context.state.logo_quality);
    }




    if (context.settings.verbose)
    {
        Debug(context, 1, "\nLogo fraction:              %.4f      %s\n",context.state.logoPercentage, ((context.settings.commDetectMethod & LOGO) ? (context.state.reverseLogoLogic? "(Reversed Logo Logic)": "") : "Logo disabled") );
        Debug(context, 1,   "Maximum volume found:       %6i\n", context.state.maxi_volume);
        Debug(context, 1,   "Average volume:             %6i\n", context.state.avg_volume);
        Debug(context, 1,   "Sound threshold:            %6i\n", context.settings.max_volume);
        Debug(context, 1,   "Silence threshold:          %6i\n", context.settings.max_silence);
        Debug(context, 1,   "Minimum volume found:       %6i\n", context.state.min_volume);
        Debug(context, 1,   "Average frames with silence:%6i\n", context.state.avg_silence);
        Debug(context, 1,   "Black threshold:            %6i\n", context.settings.max_avg_brightness);
        Debug(context, 1,   "Minimum brightness found:   %6i\n", context.state.min_brightness_found);
        Debug(context, 1,   "Minimum bright pixels found:%6i\n", context.state.min_hasBright);
        Debug(context, 1,   "Minimum dim level found:    %6i\n", context.state.min_dimCount);
        Debug(context, 1,   "Average brightness:         %6i\n", context.state.avg_brightness);
        Debug(context, 1,   "Uniformity level:           %6i\n", context.settings.non_uniformity);
        Debug(context, 1,   "Average non uniformity:     %6i\n", context.state.avg_uniform);
        Debug(context, 1,   "Maximum gap between logo's: %6i\n", context.state.max_logo_gap);
        Debug(context, 1,   "Suggested logo_threshold:   %.4f\n",context.state.logo_quality);
        Debug(context, 1,   "Suggested shrink_logo:	    %.2f\n", context.state.logo_overshoot);
        Debug(context, 1,   "Max commercial size found:  %6i\n", context.state.max_nonlogo_block_length);
        Debug(context, 1,   "Dominant aspect ratio:      %.4f\n",context.state.dominant_ar);
        Debug(context, 1,   "Score threshold:            %.4f\n", threshold);
        Debug(context, 1,   "Framerate:                  %2.3f\n", context.settings.fps);
        Debug(context, 1,   "Average framerate:          %2.3f\n", context.state.avg_fps);

        Debug(context, 1,   "Total commercial length:    %s\n",	dblSecondsToStrMinutes(context, comlength));
        Debug(context, 1,   "Cut codes:\n");
        Debug(context, 1,   "  F: scene\t c: change\n  A: aspect\t t: cutscene\n  E: exceeds\t l: logo\n  L: logo\t v: volume\n  B: bright\t s: scene_change\n  C: combined\t a: aspect_ratio\n  N: nonstrict\t u: uniform_frame\n  S: strict\t b: black_frame\n  \t\t r: resolution\n");
        Debug(context, 1,   "----------------------------------------------------\n");
        Debug(context, 1,   "Block list after weighing\n----------------------------------------------------\n", threshold);
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
                CCTypeToStr(context, context.state.cblock[i].cc_type)
            );
            if (context.settings.commDetectMethod & LOGO)
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
        context.state.training_file.reset(myfopen("strict.csv", "a+"));
//		fprintf(training_file, "// score, length, fraction, position,combined, ar error, logo, strict \n");
    }
    if (context.state.training_file.get())
        fprintf(context.state.training_file.get(), "%+f,%+f,%+f,%s\n", len,delta, tol, comskip::output::csv_field(context.state.inbasename).c_str());
}




void OutputTraining(RecordingContext& context)
{
    int i;
//	return;
    if (!context.settings.output_training)
        return;
    context.state.training_file.reset(myfopen("comskip.csv", "a+"));

#ifdef WRITEPATTERN
    r = (reffer[0].start_frame/fps < 30.0 ? reffer_count: reffer_count+1);
    if (reffer[0].start_frame/fps < 30.0)
        s = reffer[0].end_frame;
    else
        s = 0;
    fprintf(training_file, "\"%s\",%f,%d,", inbasename,  (reffer[reffer_count].start_frame - s)/fps, r);
    for (i = 0; i < 40; i++)
    {
        if (i <= reffer_count)
        {
            if (i == 0)
                e = 0;
            else
                e = reffer[i-1].end_frame;
            if (i == reffer_count)
                s = 0;
            else
                s = (reffer[i].end_frame - reffer[i].start_frame);
            if (i > 0)
                fprintf(training_file, "%f,%f,", (reffer[i].start_frame-e)/fps, s/fps);
            else
            {
                if (reffer[i].start_frame/fps > 30.0)
                    fprintf(training_file, "%f,%f, %f,%f,", 0.0, 0.0, (reffer[i].start_frame-e)/fps,s/fps);
                else
                    fprintf(training_file, "%f,%f,", (reffer[i].start_frame-e)/fps,s/fps);
            }
        }
        else
        {
            fprintf(training_file, "%f,%f,", 0.0, 0.0);
        }
    }
    fprintf(training_file, "0\n", inbasename);


    r = (context.state.commercial[0].start_frame/fps < 30.0 ? context.state.commercial_count: context.state.commercial_count+1);
    if (context.state.commercial[0].start_frame/fps < 30.0)
        s = context.state.commercial[0].end_frame;
    else
        s = 0;
    fprintf(training_file, "\"%s\",%f,%d,", inbasename,  (context.state.commercial[context.state.commercial_count].start_frame - s)/fps, r);
    for (i = 0; i < 40; i++)
    {
        if (i <= context.state.commercial_count)
        {
            if (i == 0)
                e = 0;
            else
                e = context.state.commercial[i-1].end_frame;
            if (i == context.state.commercial_count)
                s = 0;
            else
                s = (context.state.commercial[i].end_frame - context.state.commercial[i].start_frame);
            if (i > 0)
                fprintf(training_file, "%f,%f,", (context.state.commercial[i].start_frame-e)/fps, s/fps);
            else
            {
                if (context.state.commercial[i].start_frame/fps > 30.0)
                    fprintf(training_file, "%f,%f, %f,%f,", 0.0, 0.0, (context.state.commercial[i].start_frame-e)/fps,s/fps);
                else
                    fprintf(training_file, "%f,%f,", (context.state.commercial[i].start_frame-e)/fps,s/fps);
            }
        }
        else
        {
            fprintf(training_file, "%f,%f,", 0.0, 0.0);
        }
    }
    fprintf(training_file, "0\n", inbasename);

#else

#define TRAINING_LAYOUT	"%3d,%c,%c,%7.2f,%7.2f,%7.2f,%7.2f,%7.2f,%5.2f,%5.2f,\"%10s\",\"%10s\",\"%10s\",%s\n"

    fprintf(context.state.training_file.get(), "block, cm,rf, score, length, start, end, fromend ar, logo, cause, less, more\n");

    for (i = 0; i < context.state.block_count; i++)
    {
        if (context.settings.output_training)
        {
            fprintf(context.state.training_file.get(), TRAINING_LAYOUT,
                    i,
                    CheckFramesForCommercial(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                    CheckFramesForReffer(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail),
                    context.state.cblock[i].score,
                    context.state.cblock[i].length,
                    F2T(context.state.cblock[i].f_start),
                    F2T(context.state.cblock[i].f_end),
                    F2L(context.state.cblock[context.state.block_count-1].f_end, context.state.cblock[i].f_end),
                    context.state.cblock[i].ar_ratio,
                    context.state.cblock[i].logo,
                    CauseString(context, context.state.cblock[i].cause),
                    CauseString(context, context.state.cblock[i].less),
                    CauseString(context, context.state.cblock[i].more),
                    comskip::output::csv_field(context.state.inbasename).c_str());

        }
    }
#endif

}
