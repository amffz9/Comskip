#include "exit_requested.h"
#include "checked_format.h"
#include "legacy_detection.h"
#include "output/live_xml.h"
#include "logo_shrink.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

int FindBlock(RecordingContext& context, long frame)
{
    int i;
    for (i = 0; i < context.state.block_count; i++)
    {
        if ((frame >= context.state.cblock[i].f_start) && (frame < context.state.cblock[i].f_end))
        {
            return (i);
        }
    }

    return (-1);
}

void BuildCommListAsYouGo(RecordingContext& context)
{
    const double shrink_frames = comskip::detection::checked_logo_shrink_frames(
        context.settings.shrink_logo, context.settings.fps);
    std::vector<comskip::detection::LiveCandidate> candidates;
    std::string filename;
    int			commercials = 0;
    int			i;
    int			j;
    int			k;
    int			x;
    int			len;
    double		remainder;
    double		added;
    bool		oldbreak;
    bool		useLogo;
#ifdef OLD_LIVE_TV
    int local_blacklevel;
#endif
    std::vector<int> onTheFlyBlackFrame;
    int			onTheFlyBlackCount = 0;

    if (context.state.framenum_real - context.state.lastFrameCommCalculated <= 15 * context.settings.fps) return;

#ifdef OLD_LIVE_TV
    local_blacklevel = min_brightness_found + brightness_buffer;

    if (local_blacklevel < max_avg_brightness)
        local_blacklevel = max_avg_brightness;
#endif

    if (context.state.black_count > 0
#ifdef OLD_LIVE_TV
        && (context.state.black[context.state.black_count-1].brightness <= local_blacklevel)
         &&   (context.state.framenum_real > lastFrame)
#endif
            /*(black[black_count-1].frame == framenum_real) &&*/
        )
    {

        context.state.lastFrameCommCalculated = context.state.framenum_real;


        onTheFlyBlackFrame.resize(static_cast<std::size_t>(context.state.black_count));

#ifdef OLD_LIVE_TV
        Debug(7, "Building list of all frames with a brightness less than %i.\n", local_blacklevel);
#endif
        for (i = 1; i < context.state.black_count; i++) // Skip first black frame
        {
#ifdef OLD_LIVE_TV
            if (context.state.black[i].brightness <= local_blacklevel)
#else
            k = !(context.settings.commDetectMethod & LOGO) &&
                (context.state.black[i].cause & (C_b | C_u));
            if ((context.state.black[i].cause & C_v) || (context.state.black[i].cause & C_b) || (context.state.black[i].cause & C_u) )
            {

                const auto logo_window = comskip::detection::logo_scan_window(context.state.black[i].frame,
                    context.state.framenum_real, context.state.frame.size(), shrink_frames);
                for (j=logo_window.begin; !k && j < logo_window.end; j++ )
                {

                    if (!context.state.frame[j].logo_present)
                    {
                        k = true;
                        Debug(context, 11, "[%d] Cutpoint %s without logo\n",context.state.black[i].frame, CauseString(context, context.state.black[i].cause));
                        break;
                    }
                }
                if (k == false && (context.state.black[i].cause & C_v) )
                {
                    for (j=max(1,context.state.black[i].frame - context.settings.volume_slip * context.settings.fps); j < min(context.state.framenum_real, context.state.black[i].frame + context.settings.volume_slip * context.settings.fps ); j++ )
                    {
                        if (context.state.frame[j].isblack & C_b)
                        {
                            Debug(context, 11, "[%d] Silence and dark\n",context.state.black[i].frame);
                            k = true;
                        }
                    }
                }
//          if (frame[black[i].frame].currentGoodEdge < logo_threshold)
                if (k)
//            if (!frame[black[i].frame].logo_present)
#endif
                {
                    onTheFlyBlackFrame[onTheFlyBlackCount] = context.state.black[i].frame;
                    onTheFlyBlackCount++;
                }
            }
        }

        useLogo = context.settings.commDetectMethod & LOGO;

        if ((context.state.logo_block_count == -1) || (!context.state.logoInfoAvailable)) useLogo = false;

        // detect individual commercials from black frames
        for (i = 0; i < onTheFlyBlackCount; i++)
        {
            for (x = i + 1; x < onTheFlyBlackCount; x++)
            {
                int gap_length = onTheFlyBlackFrame[x] - onTheFlyBlackFrame[i];
                if (gap_length < context.settings.min_commercial_size * context.settings.fps)
                {
                    continue;
                }
                oldbreak = commercials > 0 && ((onTheFlyBlackFrame[i] - candidates[commercials - 1].end) < 10 * context.settings.fps);
                if (gap_length > context.settings.max_commercialbreak * context.settings.fps ||
                        (!oldbreak && gap_length > context.settings.max_commercial_size * context.settings.fps) ||
                        (oldbreak && (onTheFlyBlackFrame[x] - candidates[commercials - 1].end > context.settings.max_commercial_size * context.settings.fps)))
                {
                    break;
                }
                added = gap_length / context.settings.fps + context.settings.div5_tolerance;
                remainder = added - 5 * ((int)(added / 5.0));
                if ((context.settings.require_div5 != 1) || (remainder >= 0 && remainder <= 2 * context.settings.div5_tolerance))
                {
                    // look for segments in multiples of 5 seconds
                    if (oldbreak)
                    {
                        if (useLogo && context.settings.logo_present_modifier != 1 && CheckFramesForLogo(context, onTheFlyBlackFrame[x - 1], onTheFlyBlackFrame[x]))
                        {

                            candidates[commercials - 1].end = onTheFlyBlackFrame[x - 1];
#ifdef ADAPT_LIVE_COMMERCIAL
                            candidates[commercials - 1].end_index = x - 1;
#endif
                            Debug(context,
                                10,
                                "Logo detected between frames %i and %i.  Setting commercial to %i to %i.\n",
                                onTheFlyBlackFrame[x - 1],
                                onTheFlyBlackFrame[x],
                                candidates[commercials - 1].start,
                                candidates[commercials - 1].end
                            );
                        }
                        else if (onTheFlyBlackFrame[x] > candidates[commercials - 1].end + context.settings.fps)
                        {
                            candidates[commercials - 1].end = onTheFlyBlackFrame[x];
#ifdef ADAPT_LIVE_COMMERCIAL
                            candidates[commercials - 1].end_index = x;
#endif
                            Debug(context,
                                5,
                                "--start: %i, end: %i, len: %.2fs\t%.2fs\n",
                                onTheFlyBlackFrame[i],
                                onTheFlyBlackFrame[x],
                                (onTheFlyBlackFrame[x] - onTheFlyBlackFrame[i]) / context.settings.fps,
                                (candidates[commercials - 1].end - candidates[commercials - 1].start) / context.settings.fps
                            );
                        }
                    }
                    else
                    {
                        if (useLogo && context.settings.logo_present_modifier != 1 && CheckFramesForLogo(context, onTheFlyBlackFrame[i], onTheFlyBlackFrame[x]))
                        {
                            Debug(context,
                                11,
                                "Logo detected between frames %i and %i.  Skipping to next i.\n",
                                onTheFlyBlackFrame[i],
                                onTheFlyBlackFrame[x]
                            );
                            i = x - 1;	/*Gil*/
                            break;
                        }
                        else
                        {
                            Debug(context,
                                1,
                                "\n  start: %i, end: %i, len: %.2fs\n",
                                onTheFlyBlackFrame[i],
                                onTheFlyBlackFrame[x],
                                ((onTheFlyBlackFrame[x] - onTheFlyBlackFrame[i]) / context.settings.fps)
                            );
                            if (candidates.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
                                throw std::length_error("Live candidate count exceeds supported index type");
                            candidates.push_back({onTheFlyBlackFrame[i], onTheFlyBlackFrame[x], i, x});
                            commercials = static_cast<int>(candidates.size());

                            Debug(context,
                                1,
                                "\n  start: %i, end: %i, len: %is\n",
                                candidates[commercials - 1].start,
                                candidates[commercials - 1].end,
                                (int)((candidates[commercials - 1].end - candidates[commercials - 1].start) / context.settings.fps)
                            );
                        }
                    }
                    i = x - 1;
                    x = onTheFlyBlackCount;
                }
            }
        }
        Debug(context, 1, "\n");


        // print out commercial breaks skipping those that are too small or too large
        if (context.settings.output_default || context.settings.output_edl || context.settings.output_live || context.settings.output_dvrmstb)
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
                        Debug(context, 0, "ERROR writing to %s\n", context.state.out_filename.c_str());
                        comskip::request_exit(103);
                    }
                }
//				fprintf(out_file, "FILE PROCESSING COMPLETE %6li FRAMES AT %4i\n-------------------\n",frame_count-1, (int)(fps*100));
            }
            if (context.settings.output_edl)
            {
                filename = std::string(context.state.outbasename) + ".edl";
                context.state.edl_file.reset(myfopen(filename.c_str(), "wb"));
                if (!context.state.edl_file.get())
                {
                    sleep_for_ms(50L);
                    context.state.edl_file.reset(myfopen(filename.c_str(), "wb"));
                    if (!context.state.edl_file.get())
                    {
                        Debug(context, 0, "%s", context.translator.format("cutlists_write_failed", filename).c_str());
                        comskip::request_exit(103);
                    }
                }
            }
            if (context.settings.output_live)
            {
                filename = std::string(context.state.outbasename) + ".live";
                context.state.live_file.reset(myfopen(filename.c_str(), "wb"));
                if (!context.state.live_file.get())
                {
                    sleep_for_ms(50L);
                    context.state.live_file.reset(myfopen(filename.c_str(), "wb"));
                    if (!context.state.live_file.get())
                    {
                        Debug(context, 0, "%s", context.translator.format("cutlists_write_failed", filename).c_str());
                        comskip::request_exit(103);
                    }
                }
            }
            std::vector<comskip::output::FrameInterval> dvrmstb_intervals;
            comskip::detection::reset_intervals(context.state.reffer, context.state.reffer_count);
            comskip::detection::reset_intervals(context.state.commercial, context.state.commercial_count);
            for (i = 0; i < commercials; i++)
            {
                len = candidates[i].end - candidates[i].start;
                if ((len >= (int)context.settings.min_commercialbreak * context.settings.fps) && (len <= (int)context.settings.max_commercialbreak * context.settings.fps))
                {
#ifdef ADAPT_LIVE_COMMERCIAL
                    // find the middle of the scene change, max 3 seconds.
                    j = candidates[i].start_index;
                    while ((j > 0) && ((onTheFlyBlackFrame[j] - onTheFlyBlackFrame[j - 1]) == 1))
                    {

                        // find beginning
                        j--;
                    }

                    for (k = j; k < onTheFlyBlackCount; k++)
                    {

                        // find end
                        if ((onTheFlyBlackFrame[k] - onTheFlyBlackFrame[j]) > (int)(3 * context.settings.fps))
                        {
                            break;
                        }
                    }

                    x = j + (int)((k - j) / 2);
                    candidates[i].start = onTheFlyBlackFrame[x];
                    j = candidates[i].end_index;
                    if (j < onTheFlyBlackCount-1)
                    {
                        while ((j < onTheFlyBlackCount) && ((onTheFlyBlackFrame[j + 1] - onTheFlyBlackFrame[j]) == 1))
                        {
                            // find end
                            j++;
                            if (j >= onTheFlyBlackCount-1) break;
                        }
                    }
                    for (k = j; k > 0; k--)
                    {

                        // find start
                        if (onTheFlyBlackFrame[j] - (onTheFlyBlackFrame[k]) > (int)(3 * context.settings.fps))
                        {
                            break;
                        }
                    }
                    x = k + (int)((j - k) / 2);
                    candidates[i].end = onTheFlyBlackFrame[x] - 1;
#endif
                    Debug(context, 2, "Output: %i - start: %i   end: %i\n", i, candidates[i].start, candidates[i].end);
                    comskip::detection::append_interval(context.state.commercial, context.state.commercial_count, Legacy_commercial_entry{});
                    context.state.commercial[context.state.commercial_count].start_frame = candidates[i].start + context.settings.padding*context.settings.fps - context.settings.remove_before*context.settings.fps;
                    context.state.commercial[context.state.commercial_count].end_frame = candidates[i].end - context.settings.padding*context.settings.fps + context.settings.remove_after*context.settings.fps;
                    context.state.commercial[context.state.commercial_count].length = candidates[i].end-2*context.settings.padding - candidates[i].start + context.settings.remove_before + context.settings.remove_after;

                    if (context.settings.output_live) {
                        const auto& interval = context.state.commercial.back();
                        comskip::detection::append_interval(context.state.reffer, context.state.reffer_count,
                            Legacy_reffer_entry{interval.start_frame, interval.end_frame});
                    }

                    if (context.state.out_file.get())
                        fprintf(context.state.out_file.get(), "%li\t%li\n", candidates[i].start + context.settings.padding, candidates[i].end - context.settings.padding);
                    if (context.state.edl_file.get())
                        fprintf(context.state.edl_file.get(), "%.2f\t%.2f\t%d\n", (double) max(candidates[i].start + context.settings.padding - context.settings.edl_offset,0) / context.settings.fps , (double) max(candidates[i].end - context.settings.padding - context.settings.edl_offset,0) / context.settings.fps, context.settings.edl_skip_field );
                    if (context.state.live_file.get())
                        fprintf(context.state.live_file.get(), "%.2f\t%.2f\t%d\n", (double) max(candidates[i].start + context.settings.padding - context.settings.edl_offset,0) / context.settings.fps , (double) max(candidates[i].end - context.settings.padding - context.settings.edl_offset,0) / context.settings.fps, context.settings.edl_skip_field );
                    if (context.settings.output_dvrmstb)
                        dvrmstb_intervals.push_back({candidates[i].start, candidates[i].end});
                }
            }
            if (context.state.out_file.get()) fflush(context.state.out_file.get());
            if (context.state.out_file.get()) context.state.out_file.reset();
            context.state.out_file.reset();
            if (context.state.edl_file.get()) fflush(context.state.edl_file.get());
            if (context.state.edl_file.get()) context.state.edl_file.reset();
            context.state.edl_file.reset();
            if (context.state.live_file.get()) fflush(context.state.live_file.get());
            if (context.state.live_file.get()) context.state.live_file.reset();
            context.state.live_file.reset();
            if (context.settings.output_dvrmstb) {
                std::ostringstream serialized;
                comskip::output::write_live_dvrmstb(serialized, dvrmstb_intervals,
                                                   context.settings.fps, context.settings.padding);
                auto path = std::filesystem::path(std::u8string_view(
                    reinterpret_cast<const char8_t*>(context.state.outbasename.c_str())));
                path += ".xml";
                std::ofstream output(path, std::ios::binary | std::ios::trunc);
                if (!output)
                    throw std::ios_base::failure(std::string("Could not open live DVRMSTB output: ") +
                                                 context.state.outbasename + ".xml");
                try {
                    output.exceptions(std::ios::failbit | std::ios::badbit);
                    const auto text = serialized.str();
                    output.write(text.data(), static_cast<std::streamsize>(text.size()));
                    output.close();
                } catch (const std::ios_base::failure& error) {
                    throw std::ios_base::failure(std::string("Could not write live DVRMSTB output: ") +
                                                 context.state.outbasename + ".xml: " + error.what());
                }
            }

            if (context.settings.output_incommercial)
            {
                filename = std::string(context.state.workbasename) + ".incommercial";
                context.state.incommercial_file.reset(myfopen(filename.c_str(), "w"));
                if (!context.state.incommercial_file.get())
                {
                    fputs(context.translator.format("create_failed", strerror(errno), filename).c_str(), stderr);
                    goto skipit;
                }
                if(context.state.commercial_count >= 0 && context.state.commercial.back().end_frame > context.state.framenum_real - context.settings.incommercial_frames)
                    fprintf(context.state.incommercial_file.get(), "1\n");
                else
                    fprintf(context.state.incommercial_file.get(), "0\n");
                context.state.incommercial_file.reset();
skipit:
                ;
            }

        }

    }

}
