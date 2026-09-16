#include "exit_requested.h"
#include "checked_format.h"
#include "legacy_detection.h"
#include "output/live_xml.h"
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
    std::vector<long> c_start;
    std::vector<long> c_end;
#ifdef ADAPT_LIVE_COMMERCIAL
    std::vector<long> ic_start;
    std::vector<long> ic_end;
#endif
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

        c_start.resize(MAX_COMMERCIALS);
        c_end.resize(MAX_COMMERCIALS);
#ifdef ADAPT_LIVE_COMMERCIAL
        ic_start.resize(MAX_COMMERCIALS);
        ic_end.resize(MAX_COMMERCIALS);
#endif

        onTheFlyBlackFrame.resize(static_cast<std::size_t>(context.state.black_count));

#ifdef OLD_LIVE_TV
        Debug(7, "Building list of all frames with a brightness less than %i.\n", local_blacklevel);
#endif
        for (i = 1; i < context.state.black_count; i++) // Skip first black frame
        {
#ifdef OLD_LIVE_TV
            if (context.state.black[i].brightness <= local_blacklevel)
#else
            k = false;
            if ((context.state.black[i].cause & C_v) || (context.state.black[i].cause & C_b) || (context.state.black[i].cause & C_u) )
            {

                for (j=max(1,context.state.black[i].frame - context.settings.shrink_logo * context.settings.fps); j < min(context.state.framenum_real, context.state.black[i].frame + context.settings.shrink_logo * context.settings.fps ); j++ )
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
                oldbreak = commercials > 0 && ((onTheFlyBlackFrame[i] - c_end[commercials - 1]) < 10 * context.settings.fps);
                if (gap_length > context.settings.max_commercialbreak * context.settings.fps ||
                        (!oldbreak && gap_length > context.settings.max_commercial_size * context.settings.fps) ||
                        (oldbreak && (onTheFlyBlackFrame[x] - c_end[commercials - 1] > context.settings.max_commercial_size * context.settings.fps)))
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
                        if (CheckFramesForLogo(context, onTheFlyBlackFrame[x - 1], onTheFlyBlackFrame[x]) && useLogo && context.settings.logo_present_modifier != 1)
                        {

                            c_end[commercials - 1] = onTheFlyBlackFrame[x - 1];
#ifdef ADAPT_LIVE_COMMERCIAL
                            ic_end[commercials - 1] = x - 1;
#endif
                            Debug(context,
                                10,
                                "Logo detected between frames %i and %i.  Setting commercial to %i to %i.\n",
                                onTheFlyBlackFrame[x - 1],
                                onTheFlyBlackFrame[x],
                                c_start[commercials - 1],
                                c_end[commercials - 1]
                            );
                        }
                        else if (onTheFlyBlackFrame[x] > c_end[commercials - 1] + context.settings.fps)
                        {
                            c_end[commercials - 1] = onTheFlyBlackFrame[x];
#ifdef ADAPT_LIVE_COMMERCIAL
                            ic_end[commercials - 1] = x;
#endif
                            Debug(context,
                                5,
                                "--start: %i, end: %i, len: %.2fs\t%.2fs\n",
                                onTheFlyBlackFrame[i],
                                onTheFlyBlackFrame[x],
                                (onTheFlyBlackFrame[x] - onTheFlyBlackFrame[i]) / context.settings.fps,
                                (c_end[commercials - 1] - c_start[commercials - 1]) / context.settings.fps
                            );
                        }
                    }
                    else
                    {
                        if (CheckFramesForLogo(context, onTheFlyBlackFrame[i], onTheFlyBlackFrame[x]) && useLogo && context.settings.logo_present_modifier != 1)
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
#ifdef ADAPT_LIVE_COMMERCIAL
                            ic_start[commercials] = i;
                            ic_end[commercials] = x;
#endif
                            c_start[commercials] = onTheFlyBlackFrame[i];
                            c_end[commercials++] = onTheFlyBlackFrame[x];

                            Debug(context,
                                1,
                                "\n  start: %i, end: %i, len: %is\n",
                                c_start[commercials - 1],
                                c_end[commercials - 1],
                                (int)((c_end[commercials - 1] - c_start[commercials - 1]) / context.settings.fps)
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
            context.state.reffer_count = -1;
            context.state.commercial_count = -1;
            for (i = 0; i < commercials; i++)
            {
                len = c_end[i] - c_start[i];
                if ((len >= (int)context.settings.min_commercialbreak * context.settings.fps) && (len <= (int)context.settings.max_commercialbreak * context.settings.fps))
                {
#ifdef ADAPT_LIVE_COMMERCIAL
                    // find the middle of the scene change, max 3 seconds.
                    j = ic_start[i];
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
                    c_start[i] = onTheFlyBlackFrame[x];
                    j = ic_end[i];
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
                    c_end[i] = onTheFlyBlackFrame[x] - 1;
#endif
                    Debug(context, 2, "Output: %i - start: %i   end: %i\n", i, c_start[i], c_end[i]);
                    context.state.commercial_count++;
                    if (context.state.commercial_count >= MAX_COMMERCIALS)
                    {
                        Debug(context, 0, "Insufficient memory to manage live_tv commercials\n");
                        comskip::request_exit(8);
                    }
                    context.state.commercial[context.state.commercial_count].start_frame = c_start[i] + context.settings.padding*context.settings.fps - context.settings.remove_before*context.settings.fps;
                    context.state.commercial[context.state.commercial_count].end_frame = c_end[i] - context.settings.padding*context.settings.fps + context.settings.remove_after*context.settings.fps;
                    context.state.commercial[context.state.commercial_count].length = c_end[i]-2*context.settings.padding - c_start[i] + context.settings.remove_before + context.settings.remove_after;

                    if (context.settings.output_live) {
                        context.state.reffer_count++;
                        context.state.reffer[context.state.reffer_count].start_frame = context.state.commercial[context.state.reffer_count].start_frame;
                        context.state.reffer[context.state.reffer_count].end_frame = context.state.commercial[context.state.reffer_count].end_frame;
                    }

                    if (context.state.out_file.get())
                        fprintf(context.state.out_file.get(), "%li\t%li\n", c_start[i] + context.settings.padding, c_end[i] - context.settings.padding);
                    if (context.state.edl_file.get())
                        fprintf(context.state.edl_file.get(), "%.2f\t%.2f\t%d\n", (double) max(c_start[i] + context.settings.padding - context.settings.edl_offset,0) / context.settings.fps , (double) max(c_end[i] - context.settings.padding - context.settings.edl_offset,0) / context.settings.fps, context.settings.edl_skip_field );
                    if (context.state.live_file.get())
                        fprintf(context.state.live_file.get(), "%.2f\t%.2f\t%d\n", (double) max(c_start[i] + context.settings.padding - context.settings.edl_offset,0) / context.settings.fps , (double) max(c_end[i] - context.settings.padding - context.settings.edl_offset,0) / context.settings.fps, context.settings.edl_skip_field );
                    if (context.settings.output_dvrmstb)
                        dvrmstb_intervals.push_back({c_start[i], c_end[i]});
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
                if(context.state.commercial[context.state.commercial_count].end_frame > context.state.framenum_real - context.settings.incommercial_frames)
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
