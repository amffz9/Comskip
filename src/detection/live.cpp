#include "legacy_detection.h"

int FindBlock(long frame)
{
    int i;
    for (i = 0; i < block_count; i++)
    {
        if ((frame >= cblock[i].f_start) && (frame < cblock[i].f_end))
        {
            return (i);
        }
    }

    return (-1);
}

void BuildCommListAsYouGo(void)
{
    long		c_start[MAX_COMMERCIALS];
    long		c_end[MAX_COMMERCIALS];
#ifdef ADAPT_LIVE_COMMERCIAL
    long		ic_start[MAX_COMMERCIALS];
    long		ic_end[MAX_COMMERCIALS];
#endif
    char		filename[255];
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
    int*		onTheFlyBlackFrame;
    int			onTheFlyBlackCount = 0;

    if (framenum_real - lastFrameCommCalculated <= 15 * fps) return;

#ifdef OLD_LIVE_TV
    local_blacklevel = min_brightness_found + brightness_buffer;

    if (local_blacklevel < max_avg_brightness)
        local_blacklevel = max_avg_brightness;
#endif

    if (black_count > 0
#ifdef OLD_LIVE_TV
        && (black[black_count-1].brightness <= local_blacklevel)
         &&   (framenum_real > lastFrame)
#endif
            /*(black[black_count-1].frame == framenum_real) &&*/
        )
    {

        lastFrameCommCalculated = framenum_real;

        onTheFlyBlackFrame = static_cast<int *>( calloc(black_count, sizeof(int)) );
        if (onTheFlyBlackFrame == NULL)
        {
            Debug(0, "Could not allocate memory for onTheFlyBlackFrame\n");
            exit(8);
        }

#ifdef OLD_LIVE_TV
        Debug(7, "Building list of all frames with a brightness less than %i.\n", local_blacklevel);
#endif
        for (i = 1; i < black_count; i++) // Skip first black frame
        {
#ifdef OLD_LIVE_TV
            if (black[i].brightness <= local_blacklevel)
#else
            k = false;
            if ((black[i].cause & C_v) || (black[i].cause & C_b) || (black[i].cause & C_u) )
            {

                for (j=max(1,black[i].frame - shrink_logo * fps); j < min(framenum_real, black[i].frame + shrink_logo * fps ); j++ )
                {

                    if (!frame[j].logo_present)
                    {
                        k = true;
                        Debug(11, "[%d] Cutpoint %s without logo\n",black[i].frame, CauseString(black[i].cause));
                        break;
                    }
                }
                if (k == false && (black[i].cause & C_v) )
                {
                    for (j=max(1,black[i].frame - volume_slip * fps); j < min(framenum_real, black[i].frame + volume_slip * fps ); j++ )
                    {
                        if (frame[j].isblack & C_b)
                        {
                            Debug(11, "[%d] Silence and dark\n",black[i].frame);
                            k = true;
                        }
                    }
                }
//          if (frame[black[i].frame].currentGoodEdge < logo_threshold)
                if (k)
//            if (!frame[black[i].frame].logo_present)
#endif
                {
                    onTheFlyBlackFrame[onTheFlyBlackCount] = black[i].frame;
                    onTheFlyBlackCount++;
                }
            }
        }

        useLogo = commDetectMethod & LOGO;

        if ((logo_block_count == -1) || (!logoInfoAvailable)) useLogo = false;

        // detect individual commercials from black frames
        for (i = 0; i < onTheFlyBlackCount; i++)
        {
            for (x = i + 1; x < onTheFlyBlackCount; x++)
            {
                int gap_length = onTheFlyBlackFrame[x] - onTheFlyBlackFrame[i];
                if (gap_length < min_commercial_size * fps)
                {
                    continue;
                }
                oldbreak = commercials > 0 && ((onTheFlyBlackFrame[i] - c_end[commercials - 1]) < 10 * fps);
                if (gap_length > max_commercialbreak * fps ||
                        (!oldbreak && gap_length > max_commercial_size * fps) ||
                        (oldbreak && (onTheFlyBlackFrame[x] - c_end[commercials - 1] > max_commercial_size * fps)))
                {
                    break;
                }
                added = gap_length / fps + div5_tolerance;
                remainder = added - 5 * ((int)(added / 5.0));
                if ((require_div5 != 1) || (remainder >= 0 && remainder <= 2 * div5_tolerance))
                {
                    // look for segments in multiples of 5 seconds
                    if (oldbreak)
                    {
                        if (CheckFramesForLogo(onTheFlyBlackFrame[x - 1], onTheFlyBlackFrame[x]) && useLogo && logo_present_modifier != 1)
                        {

                            c_end[commercials - 1] = onTheFlyBlackFrame[x - 1];
#ifdef ADAPT_LIVE_COMMERCIAL
                            ic_end[commercials - 1] = x - 1;
#endif
                            Debug(
                                10,
                                "Logo detected between frames %i and %i.  Setting commercial to %i to %i.\n",
                                onTheFlyBlackFrame[x - 1],
                                onTheFlyBlackFrame[x],
                                c_start[commercials - 1],
                                c_end[commercials - 1]
                            );
                        }
                        else if (onTheFlyBlackFrame[x] > c_end[commercials - 1] + fps)
                        {
                            c_end[commercials - 1] = onTheFlyBlackFrame[x];
#ifdef ADAPT_LIVE_COMMERCIAL
                            ic_end[commercials - 1] = x;
#endif
                            Debug(
                                5,
                                "--start: %i, end: %i, len: %.2fs\t%.2fs\n",
                                onTheFlyBlackFrame[i],
                                onTheFlyBlackFrame[x],
                                (onTheFlyBlackFrame[x] - onTheFlyBlackFrame[i]) / fps,
                                (c_end[commercials - 1] - c_start[commercials - 1]) / fps
                            );
                        }
                    }
                    else
                    {
                        if (CheckFramesForLogo(onTheFlyBlackFrame[i], onTheFlyBlackFrame[x]) && useLogo && logo_present_modifier != 1)
                        {
                            Debug(
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
                            Debug(
                                1,
                                "\n  start: %i, end: %i, len: %.2fs\n",
                                onTheFlyBlackFrame[i],
                                onTheFlyBlackFrame[x],
                                ((onTheFlyBlackFrame[x] - onTheFlyBlackFrame[i]) / fps)
                            );
#ifdef ADAPT_LIVE_COMMERCIAL
                            ic_start[commercials] = i;
                            ic_end[commercials] = x;
#endif
                            c_start[commercials] = onTheFlyBlackFrame[i];
                            c_end[commercials++] = onTheFlyBlackFrame[x];

                            Debug(
                                1,
                                "\n  start: %i, end: %i, len: %is\n",
                                c_start[commercials - 1],
                                c_end[commercials - 1],
                                (int)((c_end[commercials - 1] - c_start[commercials - 1]) / fps)
                            );
                        }
                    }
                    i = x - 1;
                    x = onTheFlyBlackCount;
                }
            }
        }
        Debug(1, "\n");


        // print out commercial breaks skipping those that are too small or too large
        if (output_default || output_edl || output_live || output_dvrmstb)
        {
            if (output_default)
            {
                out_file = myfopen(out_filename, "w");
                if (!out_file)
                {
                    Sleep(50L);
                    out_file = myfopen(out_filename, "w");
                    if (!out_file)
                    {
                        Debug(0, "ERROR writing to %s\n", out_filename);
                        exit(103);
                    }
                }
//				fprintf(out_file, "FILE PROCESSING COMPLETE %6li FRAMES AT %4i\n-------------------\n",frame_count-1, (int)(fps*100));
            }
            if (output_edl)
            {
                sprintf(filename, "%s.edl", outbasename);
                edl_file = myfopen(filename, "wb");
                if (!edl_file)
                {
                    Sleep(50L);
                    edl_file = myfopen(filename, "wb");
                    if (!edl_file)
                    {
                        Debug(0, "ERROR writing to %s\n", filename);
                        exit(103);
                    }
                }
            }
            if (output_live)
            {
                sprintf(filename, "%s.live", outbasename);
                live_file = myfopen(filename, "wb");
                if (!live_file)
                {
                    Sleep(50L);
                    live_file = myfopen(filename, "wb");
                    if (!live_file)
                    {
                        Debug(0, "ERROR writing to %s\n", filename);
                        exit(103);
                    }
                }
            }
            dvrmstb_file = 0;
            if (output_dvrmstb)
            {
                sprintf(filename, "%s.xml", outbasename);
                dvrmstb_file = myfopen(filename, "w");
                if (dvrmstb_file)
                {
                    //			fclose(dvrmstb_file);
                    fprintf(dvrmstb_file, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<root>\n");
                }
                else
                {
                    fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
                    exit(6);
                }
            }
            reffer_count = -1;
            commercial_count = -1;
            for (i = 0; i < commercials; i++)
            {
                len = c_end[i] - c_start[i];
                if ((len >= (int)min_commercialbreak * fps) && (len <= (int)max_commercialbreak * fps))
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
                        if ((onTheFlyBlackFrame[k] - onTheFlyBlackFrame[j]) > (int)(3 * fps))
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
                        if (onTheFlyBlackFrame[j] - (onTheFlyBlackFrame[k]) > (int)(3 * fps))
                        {
                            break;
                        }
                    }
                    x = k + (int)((j - k) / 2);
                    c_end[i] = onTheFlyBlackFrame[x] - 1;
#endif
                    Debug(2, "Output: %i - start: %i   end: %i\n", i, c_start[i], c_end[i]);
                    commercial_count++;
                    if (commercial_count >= MAX_COMMERCIALS)
                    {
                        Debug(0, "Insufficient memory to manage live_tv commercials\n");
                        exit(8);
                    }
                    commercial[commercial_count].start_frame = c_start[i] + padding*fps - remove_before*fps;
                    commercial[commercial_count].end_frame = c_end[i] - padding*fps + remove_after*fps;
                    commercial[commercial_count].length = c_end[i]-2*padding - c_start[i] + remove_before + remove_after;

                    if (output_live) {
                        reffer_count++;
                        reffer[reffer_count].start_frame = commercial[reffer_count].start_frame;
                        reffer[reffer_count].end_frame = commercial[reffer_count].end_frame;
                    }

                    if (out_file)
                        fprintf(out_file, "%li\t%li\n", c_start[i] + padding, c_end[i] - padding);
                    if (edl_file)
                        fprintf(edl_file, "%.2f\t%.2f\t%d\n", (double) max(c_start[i] + padding - edl_offset,0) / fps , (double) max(c_end[i] - padding - edl_offset,0) / fps, edl_skip_field );
                    if (live_file)
                        fprintf(live_file, "%.2f\t%.2f\t%d\n", (double) max(c_start[i] + padding - edl_offset,0) / fps , (double) max(c_end[i] - padding - edl_offset,0) / fps, edl_skip_field );
                    if (dvrmstb_file)
                        fprintf(dvrmstb_file, "  <commercial start=\"%f\" end=\"%f\" />\n", (double) (c_start[i] + padding) / fps , (double) (c_end[i] - padding) / fps);
                }
            }
            if (out_file) fflush(out_file);
            if (out_file) fclose(out_file);
            out_file = 0;
            if (edl_file) fflush(edl_file);
            if (edl_file) fclose(edl_file);
            edl_file = 0;
            if (live_file) fflush(live_file);
            if (live_file) fclose(live_file);
            live_file = 0;
            if (dvrmstb_file)
            {
                fprintf(dvrmstb_file, " </root>\n");
                fclose(dvrmstb_file);
                dvrmstb_file = 0;
            }

            if (output_incommercial)
            {
                sprintf(filename, "%s.incommercial", workbasename);
                incommercial_file = myfopen(filename, "w");
                if (!incommercial_file)
                {
                    fprintf(stderr, "%s - could not create file %s\n", strerror(errno), filename);
                    goto skipit;
                }
                if(commercial[commercial_count].end_frame > framenum_real - incommercial_frames)
                    fprintf(incommercial_file, "1\n");
                else
                    fprintf(incommercial_file, "0\n");
                fclose(incommercial_file);
skipit:
                ;
            }

        }

        free(onTheFlyBlackFrame);
    }

}

