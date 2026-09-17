#include "legacy_detection.h"
#include "frame_causes.h"
#include "black_frame_run.h"
#include "logo_histogram.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <string_view>
#include <utility>

namespace {
template <typename... Args>
void BlocksDebug(RecordingContext& context, int level, const char* key, Args&&... args)
{
    Debug(context, level, "%s", context.translator.format(key, std::forward<Args>(args)...).c_str());
}
}

char *CauseString(RecordingContext& context, int i)
{
    char *c = &(context.state.CauseString_cs[context.state.CauseString_ii][0]);
    char *rc = &(context.state.CauseString_cs[context.state.CauseString_ii][0]);
    constexpr std::array<std::pair<long, char>, 8> history_flags{
        std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_8), '8'}, std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_7), '7'},
        std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_6), '6'}, std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_5), '5'},
        std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_4), '4'}, std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_3), '3'},
        std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_2), '2'}, std::pair<long, char>{comskip::detection::cause_value(comskip::detection::BlockCause::history_1), '1'},
    };
    for (const auto [flag, marker] : history_flags)
        *c++ = (i & flag) ? marker : ' ';

    const std::string_view history{context.state.CauseString_cs[context.state.CauseString_ii], 7};
    if (history != "       ")
        *c++ = '{';
    else
        *c++ = ' ';
    *c++ = (i & comskip::detection::cause_value(comskip::detection::BlockCause::scene_change)		? 'F' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::BlockCause::aspect_ratio)		? 'A' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::BlockCause::exceeds)	? 'E' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::BlockCause::logo)		? 'L' : (i & comskip::detection::cause_value(comskip::detection::BlockCause::bright)			? 'B': ' '));
    *c++ = (i & comskip::detection::cause_value(comskip::detection::BlockCause::combined) ? 'C' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::BlockCause::non_strict)? 'N' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::BlockCause::strict)	? 'S' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::FrameCause::caption)			? 'c' : (i & comskip::detection::cause_value(comskip::detection::FrameCause::cutscene)			? 't': ' '));
    *c++ = (i & comskip::detection::cause_value(comskip::detection::FrameCause::logo)			? 'l' : (i & comskip::detection::cause_value(comskip::detection::FrameCause::silence)			? 'v': ' '));
    *c++ = (i & comskip::detection::cause_value(comskip::detection::FrameCause::scene_change)			? 's' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::FrameCause::aspect_ratio)			? 'a' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::FrameCause::non_uniform)			? 'u' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::FrameCause::black)			? 'b' : ' ');
    *c++ = (i & comskip::detection::cause_value(comskip::detection::FrameCause::resolution_change)			? 'r' : ' ');
    *c++ = 0;

    context.state.CauseString_ii = (context.state.CauseString_ii + 1) % 4;

    return(rc);
}

double ValidateBlackFrames(RecordingContext& context, long reason, double ratio, int remove)
{
    int i,k,j,last;
    int prev_cause;
    int strict_count = 0;
    int negative_count = 0;
    int positive_count = 0;
    int count = 0;
    int total_cause;
    double length,summed_length;
    int incommercial;
    const char *r = context.translator.text("blocks_reason_undefined");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::black))
        r = context.translator.text("blocks_reason_black_frame");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::silence))
        r = context.translator.text("blocks_reason_volume");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::scene_change))
        r = context.translator.text("blocks_reason_scene_change");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::caption))
        r = context.translator.text("blocks_reason_change");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::non_uniform))
        r = context.translator.text("blocks_reason_uniform_frame");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::aspect_ratio))
        r = context.translator.text("blocks_reason_aspect_ratio");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::cutscene))
        r = context.translator.text("blocks_reason_cut_scene");
    if (reason == comskip::detection::cause_value(comskip::detection::FrameCause::logo))
        r = context.translator.text("blocks_reason_logo");

    if (ratio == 0.0)
        return(0.0);
#ifndef undef
    incommercial = 0;
    i = 0; // search for reason
    strict_count = 0;
    count = 0;
    last = 0;
    length = 0.0;
    summed_length = 0.0;
    while(i < context.state.black_count)
    {
        while (i < context.state.black_count && (context.state.black[i].cause & reason) == 0)
        {
            i++;
        }
        k = static_cast<int>(comskip::detection::contiguous_black_frame_run_end(
            std::span<const black_frame_info>{context.state.black.data(),
                                              static_cast<std::size_t>(context.state.black_count)},
            static_cast<std::size_t>(i), static_cast<int>(reason)));
        if (i < context.state.black_count)
        {
            length = F2T(context.state.black[(i+k)/2].frame) - F2T(context.state.black[last].frame);
            if (length > context.settings.max_commercial_size)
            {
                if (incommercial)
                {
                    incommercial = 0;
                    if (summed_length < context.settings.min_commercialbreak && summed_length > 4.7 && context.state.black[(i+k)/2].frame < context.state.frame_count * 6 / 7  && context.state.black[last].frame > context.state.frame_count * 1 / 7 )
                    {
                        negative_count++;
                        BlocksDebug(context, 10, "blocks_negative_cutpoint_too_short", r,
                                    std::format("{:6}", context.state.black[last].frame));
                    }
                    else
                        positive_count++;
                    summed_length = 0.0;
                }
                else
                {
                    positive_count++;
                }
                summed_length = 0.0;
            }
            else
            {
                summed_length += length;
                if (incommercial && summed_length > context.settings.max_commercialbreak )
                {
                    if (context.state.black[(i+k)/2].frame < context.state.frame_count * 6 / 7)
                    {
                        negative_count++;
                        BlocksDebug(context, 10, "blocks_negative_cutpoint_too_long", r,
                                    std::format("{:6}", context.state.black[(i+k)/2].frame));
                    }
                }
                else
                {
                    positive_count++;
                    incommercial = 1;
                }
            }
        }
        last = (i+k)/2;
        i = k+1;
    }
    BlocksDebug(context, 1, "blocks_cut_distribution", r, std::format("{:3}", positive_count),
                std::format("{:3}", negative_count),
                std::format("{:6.4f}", negative_count > 0
                    ? static_cast<double>(positive_count) / static_cast<double>(negative_count)
                    : 9.99));

    if ((context.state.logoPercentage < context.settings.logo_fraction || context.state.logoPercentage > context.settings.logo_percentile) && negative_count > 1)
    {

        BlocksDebug(context, 1, "blocks_cut_confidence_without_logo", r,
                    std::format("{:3}", negative_count));
        if (remove)
        {
            for (k = context.state.black_count - 1; k >= 0; k--)
            {
                if (context.state.black[k].cause & reason)
                {
                    context.state.black[k].cause &= ~reason;
                    if (context.state.black[k].cause == 0)
                    {
                        for (j = k; j < context.state.black_count - 1; j++)
                        {
                            context.state.black[j] = context.state.black[j + 1];
                        }
                        context.state.black_count--;
                    }
                }
            }
        }

        /*

            if (negative_count > 1 && reason == comskip::detection::cause_value(comskip::detection::FrameCause::silence))
            {
                Debug(1, "Too mutch Silence Frames, disabling silence detection\n");
                commDetectMethod &= ~SILENCE;
            }
            if (negative_count > 1 && reason == comskip::detection::cause_value(comskip::detection::FrameCause::scene_change))
            {
                Debug(1, "Too mutch Scene Change, disabling Scene Change detection\n");
                commDetectMethod &= ~SCENE_CHANGE;
            }
        */
    }


#endif

    i = 1;
    strict_count = 0;
    count = 0;
    prev_cause = 0;
    while(i < context.state.black_count)
    {
        total_cause = context.state.black[i].cause;
        k = i;
        while (k + 1 < context.state.black_count &&
               context.state.black[k + 1].frame == context.state.black[k].frame + 1)
        {
            k++;
            total_cause |= context.state.black[k].cause;
        }
        last = (i+k)/2;
        if ((total_cause & reason) && (prev_cause & reason))
        {
            j = i-1;
            while (j > 0 && context.state.black[j-1].frame == context.state.black[j].frame - 1)
                j--;

            length = F2T(context.state.black[i].frame) - F2T(context.state.black[(i-1+j)/2].frame);
            if (length > 1.0 && length< context.settings.max_commercial_size)
            {
                count++;
                if (IsStandardCommercialLength(context, length, F2T(i) - F2T(j)  + 0.8 , false))
                {
//				if (length > max_commercial_size) {
                    strict_count++;
                }
            }
        }
        prev_cause = reason;
        k++;
        i = k;
    }

    if (strict_count < 2 || 100*strict_count < 100*count / ratio)
    {
        BlocksDebug(context, 1, "blocks_cut_confidence_too_low", r,
                    std::format("{:3}", strict_count), std::format("{:3}", count));
        if (remove)
        {
            for (k = context.state.black_count - 1; k >= 0; k--)
            {
                if (context.state.black[k].cause & reason)
                {
                    context.state.black[k].cause &= ~reason;
                    if (context.state.black[k].cause == 0)
                    {
                        for (j = k; j < context.state.black_count - 1; j++)
                        {
                            context.state.black[j] = context.state.black[j + 1];
                        }
                        context.state.black_count--;
                    }
                }
            }
        }
    }
    else
        BlocksDebug(context, 1, "blocks_cut_confidence", r,
                    std::format("{:3}", strict_count), std::format("{:3}", count));
    return (count > 0 ? (double)strict_count / (double) count : 0);
}

//Function code blocks

bool BuildBlocks(RecordingContext& context, bool recalc)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int a = 0;
    int count;
    int v_count;
    int b_count;
    int black_start;
    int black_end;
    int cause = 0;
    int black_threshold;
    int uniform_threshold;
    int prev_start = 1;
    int prev_head = 0;

    long b_start, b_end, b_counted;
//	char *t = "";

    comskip::detection::reset_blocks(context.state.cblock, context.state.block_count);

    context.state.recalculate = recalc;
    InitializeBlockArray(context, 0);

    // If there are no black frames, nothing can be done
//	if (!black_count && !ar_block_count) return (false);

//	OutputHistogram(volumeHistogram, volumeScale, "Volume", true);

    if (!recalc)
    {
        // Eliminate frames that are too bright from black frame list
        if (context.settings.intelligent_brightness)
        {
            OutputbrightHistogram(context);
            context.settings.max_avg_brightness = black_threshold = FindBlackThreshold(context, context.settings.black_percentile);
            BlocksDebug(context, 1, "blocks_setting_brightness_threshold",
                        std::format("{}", black_threshold));
        }
        if ((context.settings.intelligent_brightness && context.settings.non_uniformity > 0)
//			|| 	(commDetectMethod & BLACK_FRAME && non_uniformity == 0) // Diabled
           )
        {
            OutputuniformHistogram (context);
            context.settings.non_uniformity = uniform_threshold = FindUniformThreshold(context, context.settings.uniform_percentile);
            BlocksDebug(context, 1, "blocks_setting_uniform_threshold",
                        std::format("{}", uniform_threshold));

            if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::black_frame))
            {
                for (i = 1; i < context.state.frame_count; i++)
                {
                    context.state.frame[i].isblack &= ~comskip::detection::cause_value(comskip::detection::FrameCause::non_uniform);
                    if (/*!(frame[i].isblack & comskip::detection::cause_value(comskip::detection::FrameCause::black)) && */ context.settings.non_uniformity > 0 && context.state.frame[i].uniform < context.settings.non_uniformity && context.state.frame[i].brightness < 250 /*&& frame[i].volume < max_volume*/ )
                        InsertBlackFrame(context, i,context.state.frame[i].brightness,context.state.frame[i].uniform,context.state.frame[i].volume, (int)comskip::detection::cause_value(comskip::detection::FrameCause::non_uniform));
                }
            }
        }
    }

    j = 0;
    for (i = 2; i < context.state.frame_count - 1; i++)  // frame 0 is not used
        if (context.state.frame[i-1].volume != -1 && context.state.frame[i].volume == -1 && context.state.frame[i+1].volume != -1)
            j++;
    if (j>0)
        BlocksDebug(context, 9, "blocks_single_missing_audio_frames", std::format("{}", j));

    if (context.settings.non_uniformity < context.state.min_uniform + 100)
        context.settings.non_uniformity = context.state.min_uniform + 100;

    if (context.state.framearray)  						// Find minumum volume around black frame
    {
        for (k = context.state.black_count - 1; k >= 0; k--)
        {
            if (context.state.black[k].cause == comskip::detection::cause_value(comskip::detection::FrameCause::scene_change) || context.state.black[k].cause == comskip::detection::cause_value(comskip::detection::FrameCause::caption) || context.state.black[k].cause == (comskip::detection::cause_value(comskip::detection::FrameCause::caption)|comskip::detection::cause_value(comskip::detection::FrameCause::scene_change)) )
            {
                i = context.state.black[k].frame-context.settings.volume_slip;		// Find quality of silence around black frame
                if (i < 0) i = 0;
                j = context.state.black[k].frame+context.settings.volume_slip;
                if (j>context.state.frame_count) j = context.state.frame_count;
                count = 0;
                for (a=i; a<j; a++)
                {
                    if (context.state.frame[a].volume < context.settings.max_volume/4)
                        count += context.settings.volume_slip;
                    else if (context.state.frame[a].volume < context.settings.max_volume)
                        count++;

                }
                if (count > context.settings.volume_slip/4)
                {
                    context.state.black[k].volume = context.settings.max_volume /2;
                }
                else
                {
                    context.state.black[k].volume = context.settings.max_volume *10 ;
                }
            }
            else
            {
                i = context.state.black[k].frame-context.settings.volume_slip;		// Find minimum volume around black frame
                if (i < 0) i = 0;
                j = context.state.black[k].frame+context.settings.volume_slip;
                if (j>context.state.frame_count) j = context.state.frame_count;
                for (a=i; a<j; a++)
                    if (context.state.frame[a].volume >= 0)
                        if (context.state.black[k].volume > context.state.frame[a].volume)
                            context.state.black[k].volume = context.state.frame[a].volume;
            }
        }
        for (k = context.state.ar_block_count - 1; k >= 0; k--)
        {
            i = context.state.ar_block[k].end-context.settings.volume_slip;
            if (i < 0) i = 0;
            j = context.state.ar_block[k].end+context.settings.volume_slip;
            if (j>context.state.frame_count) j = context.state.frame_count;
            context.state.ar_block[k].volume = context.state.frame[i].volume;
            for (a=i; a<j; a++)
                if (context.state.frame[a].volume >= 0)
                    if (context.state.ar_block[k].volume > context.state.frame[a].volume)
                    {
                        context.state.ar_block[k].volume = context.state.frame[a].volume;
                        context.state.ar_block[k].end = a;
                        if (k < context.state.ar_block_count - 1)
                            context.state.ar_block[k+1].start = a;
                    }
        }

    }
    for (k = 1; k < 255; k++)
    {
        if (context.state.volumeHistogram[k] > 10)
        {
            context.state.min_volume = (k-1)*context.state.volumeScale;
            break;
        }
    }

    for (k = 1; k < 255; k++)
    {
        if (context.state.uniformHistogram[k] > 10)
        {
            context.state.min_uniform = (k-1)*UNIFORMSCALE;
            break;
        }
    }

    for (k = 0; k < 255; k++)
    {
        if (context.state.brightHistogram[k] > 1)
        {
            context.state.min_brightness_found = k;
            break;
        }
    }
    if (context.settings.max_volume > 0)
    {
        for (k = context.state.black_count - 1; k >= 0; k--)
        {
            if ((context.state.black[k].cause & comskip::detection::cause_value(comskip::detection::FrameCause::cutscene)) != 0)
                continue;
            if (context.state.black[k].volume >  context.settings.max_volume
//				&&
//				( black[k].frame > 10 && (int)frame[black[k].frame-2].brightness < (int)frame[black[k].frame].brightness + 50 &&
//				 black[k].frame < frame_count - 10 && (int)frame[black[k].frame+2].brightness < (int) frame[black[k].frame].brightness + 50 )
//			   || black[k].volume >  max_volume * 1.5
               )
            {

                BlocksDebug(context, 12, "blocks_remove_loud_black_frame",
                    k,
                    context.state.black[k].frame,
                    context.state.black[k].volume,
                    context.settings.max_volume,
                    context.state.black[k].brightness,
                    context.state.black[k].uniform
                );

                for (j = k; j < context.state.black_count - 1; j++)
                {
                    context.state.black[j] = context.state.black[j + 1];
                }

                context.state.black_count--;
            }
        }
    }

    for (k = context.state.black_count - 1; k >= 0; k--)
    {
        if ((context.state.black[k].cause & comskip::detection::cause_value(comskip::detection::FrameCause::cutscene)) != 0)
            continue;

        if ((context.state.black[k].cause & comskip::detection::cause_value(comskip::detection::FrameCause::resolution_change)) != 0)
            continue;

        if ((context.state.black[k].cause & comskip::detection::cause_value(comskip::detection::FrameCause::black)) && context.state.black[k].brightness > context.settings.max_avg_brightness)
        {

            BlocksDebug(context, 12, "blocks_remove_bright_black_frame",
                        k,
                        context.state.black[k].frame,
                        context.state.black[k].brightness,
                        context.settings.max_avg_brightness,
                        context.state.black[k].uniform
                        );

            for (j = k; j < context.state.black_count - 1; j++)
            {
                context.state.black[j] = context.state.black[j + 1];
            }

            context.state.black_count--;
        }
    }
    if (context.settings.non_uniformity > 0)
    {
        for (k = context.state.black_count - 1; k >= 0; k--)
        {
            if ((context.state.black[k].cause & comskip::detection::cause_value(comskip::detection::FrameCause::cutscene)) != 0)
                continue;
            if ((context.state.black[k].cause & comskip::detection::cause_value(comskip::detection::FrameCause::non_uniform)) && context.state.black[k].uniform > context.settings.non_uniformity)
            {
                BlocksDebug(context, 12, "blocks_remove_nonuniform_frame",
                k,
                context.state.black[k].frame,
                context.state.black[k].uniform,
                context.settings.non_uniformity,
                context.state.black[k].brightness
                );

                for (j = k; j < context.state.black_count - 1; j++)
                {
                    context.state.black[j] = context.state.black[j + 1];
                }

                context.state.black_count--;
            }
        }
    }

    if (context.settings.cut_on_ar_change==2)
    {
        if (context.state.logoPercentage > context.settings.logo_fraction && context.state.logoPercentage < context.settings.logo_percentile)
            context.settings.cut_on_ar_change = 1;
//		else
//			ar_wrong_modifier=1;
    }

    if (context.settings.cut_on_ar_change==1)
    {
//		if (logoPercentage < logo_fraction || logoPercentage > logo_percentile)
//			cut_on_ar_change = 2;
    }

    if ((comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo) && context.settings.cut_on_ar_change ) || context.settings.cut_on_ar_change >= 2)
    {
//	if (cut_on_ar_change ) {
        for (i = 0; i < context.state.ar_block_count; i++)
        {
            if ((context.settings.cut_on_ar_change == 1 || context.state.ar_block[i].volume < context.settings.max_volume) &&
                    context.state.ar_block[i].ar_ratio != AR_UNDEF && context.state.ar_block[i+1].ar_ratio != AR_UNDEF)
            {
                a = context.state.ar_block[i].end;
//					if (a > 20 * fps)
                InsertBlackFrame(context, a,context.state.frame[a].brightness,context.state.frame[a].uniform,context.state.frame[a].volume, comskip::detection::cause_value(comskip::detection::FrameCause::aspect_ratio));
            }
        }
    }

    if ( context.settings.cut_on_ac_change )
    {
        for (i = 0; i < context.state.ac_block_count; i++)
        {
            a = context.state.ac_block[i].end;
            InsertBlackFrame(context, a,context.state.frame[a].brightness,context.state.frame[a].uniform,context.state.frame[a].volume, comskip::detection::cause_value(comskip::detection::FrameCause::resolution_change));
        }
    }


    if (ValidateBlackFrames(context, comskip::detection::cause_value(comskip::detection::FrameCause::black), 3.0, false) < 1 / 3.0)
        BlocksDebug(context, 8, "blocks_black_frame_cutting_too_low");

    if (context.settings.validate_scenechange /* || (logoPercentage < logo_fraction || logoPercentage > logo_percentile) */)
        ValidateBlackFrames(context, comskip::detection::cause_value(comskip::detection::FrameCause::scene_change), ((context.state.logoPercentage < context.settings.logo_fraction || context.state.logoPercentage > context.settings.logo_percentile) ? 1.2 : 3.5), true);

    //	ValidateBlackFrames(comskip::detection::cause_value(comskip::detection::FrameCause::caption), 3.0, true);

    if (context.settings.validate_uniform)
        ValidateBlackFrames(context, comskip::detection::cause_value(comskip::detection::FrameCause::non_uniform), ((context.state.logoPercentage < context.settings.logo_fraction || context.state.logoPercentage > context.settings.logo_percentile) ? 1.2 : 3.0), true);


    if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::silence))
    {
        k = 0;
        for (i = 0; i < context.state.frame_count; i++)
        {
            if (context.state.frame[i].volume < context.settings.max_volume) k++;
        }
        /*
                if (k * 100 / frame_count > 25) {
                    Debug(8, "Too mutch Silence Frames (%d%%), disabling silence detection\n", k * 100 / frame_count);

                    ValidateBlackFrames(comskip::detection::cause_value(comskip::detection::FrameCause::silence), 1.0, true);
                    commDetectMethod &= ~SILENCE;
                    validate_silence = 0;
                } else
        */		if (context.settings.validate_silence)
            ValidateBlackFrames(context, comskip::detection::cause_value(comskip::detection::FrameCause::silence), 3.0, true);
    }

//		if (logoPercentage < logo_fraction)
//	if (cut_on_ar_change == 2)
//		ValidateBlackFrames(comskip::detection::cause_value(comskip::detection::FrameCause::aspect_ratio), 3.0, true);


    BlocksDebug(context, 8, "blocks_black_frame_list_heading", context.state.black_count);
    for (k = 0; k < context.state.black_count; k++)
    {
        Debug(context, 8, "%3i\t%6i\t%8.3f\t%6i\t%6i\t%6i\t%6s\t%6i\t%6i\t%c\n", k, context.state.black[k].frame, get_frame_pts(context, context.state.black[k].frame), context.state.black[k].brightness, context.state.black[k].uniform, context.state.black[k].volume,&(CauseString(context, context.state.black[k].cause)[10]), context.state.frame[context.state.black[k].frame].dimCount, context.state.frame[context.state.black[k].frame].hasBright, context.state.frame[context.state.black[k].frame].pict_type);
        if (k+1 < context.state.black_count && context.state.black[k].frame+1 != context.state.black[k+1].frame)
            Debug(context, 8, "-----------------------------\n");

    }


    // add black frame at end to enable usage of last cblock
    InsertBlackFrame(context, context.state.framesprocessed,0,0,0,comskip::detection::cause_value(comskip::detection::FrameCause::black));
    /*
        InitializeBlackArray(black_count);
        black[black_count].frame = framesprocessed;
        black[black_count].brightness = 0;
        black[black_count].uniform = 0;
        black[black_count].volume = 0;
        black[black_count].cause = 0;
        black_count++;
    */
    //Create blocks



    i = 0;
    j = 0;
//	if (((commDetectMethod & LOGO) && cut_on_ar_change ) || cut_on_ar_change == 2)
//		a = 0;
//	else
    a = context.state.ar_block_count;			// Don't cut on AR when logo disabled
    cause = 0;
    context.state.block_count = 0;
    prev_start = 1;
    prev_head = 0;

    while(i < context.state.black_count || a < context.state.ar_block_count)
    {
        if (!comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo) && i < context.state.black_count && (context.state.black[i].cause & (comskip::detection::cause_value(comskip::detection::FrameCause::scene_change) | comskip::detection::cause_value(comskip::detection::FrameCause::logo))))
        {
//			i++; // Skip logo cuts and brighness cuts when not enough logo detected
//			goto again;
        }
        cause = 0;
        b_start = context.state.black[i].frame;
        cause |= context.state.black[i].cause;
        b_end = b_start;
        j = i + 1;

        v_count = 0;
        b_count = 0;
        black_start = 0;
        black_end = 0;
        //Find end of next black cblock
        while(j < context.state.black_count && (F2T(context.state.black[j].frame) - F2T(b_end) < 1.0 ))   //Allow for 2 missing black frames
        {
            if (context.state.black[j].frame - b_end > 2 &&
                    (((context.state.black[j].cause & (comskip::detection::cause_value(comskip::detection::FrameCause::silence))) != 0 &&  (cause & (comskip::detection::cause_value(comskip::detection::FrameCause::silence))) == 0) ||
                     ((context.state.black[j].cause & (comskip::detection::cause_value(comskip::detection::FrameCause::silence))) == 0 &&  (cause & (comskip::detection::cause_value(comskip::detection::FrameCause::silence))) != 0)))
            {

                BlocksDebug(context, 6, "blocks_black_frame_gap",
                    context.state.black[j].frame,
                    context.state.black[j].frame - b_end
                );
            }

            if ((context.state.black[j].cause & (comskip::detection::cause_value(comskip::detection::FrameCause::black) | comskip::detection::cause_value(comskip::detection::FrameCause::scene_change) | comskip::detection::cause_value(comskip::detection::FrameCause::non_uniform) | comskip::detection::cause_value(comskip::detection::FrameCause::resolution_change))) != 0)
            {
                b_count++;
                if (black_start == 0)
                    black_start = context.state.black[j].frame;
                black_end = context.state.black[j].frame;

            }
            if ((context.state.black[j].cause & (comskip::detection::cause_value(comskip::detection::FrameCause::silence))) != 0)
                v_count++;
            if (context.state.black[j].cause == comskip::detection::cause_value(comskip::detection::FrameCause::aspect_ratio))
            {
                cause |= context.state.black[j].cause;
                j++;
            }
            else if (cause == comskip::detection::cause_value(comskip::detection::FrameCause::aspect_ratio))
            {
                cause |= context.state.black[j].cause;
                b_start = b_end = context.state.black[j++].frame;
            }
            else
            {
                cause |= context.state.black[j].cause;
                b_end = context.state.black[j++].frame;
            }
        }
        i = j;

        if (b_count > 0 && v_count > 1.5*b_count)
        {
            b_start = black_start;
            b_end = black_end;
            b_counted = b_count;
        }
        else if (b_count == 0 && v_count > 5)
        {
            b_start = b_start - 1 + v_count / 2;
            b_end = b_end + 1 - v_count / 2;
            b_counted = 3;
        }
        context.state.cblock[context.state.block_count].cause = cause;

        //Do it this way for in roundoff problems
        b_counted = (b_end - b_start + 1)/2;

        context.state.cblock[context.state.block_count].b_head = prev_head;
        context.state.cblock[context.state.block_count].f_start = prev_start - context.state.cblock[context.state.block_count].b_head;
        if (b_end == context.state.framesprocessed)
            context.state.cblock[context.state.block_count].f_end = context.state.framesprocessed;
        else
            context.state.cblock[context.state.block_count].f_end = b_start + b_counted - 1;
        context.state.cblock[context.state.block_count].b_tail = b_counted;		//half on the tail of this cblock
        context.state.cblock[context.state.block_count].bframe_count = context.state.cblock[context.state.block_count].b_head + context.state.cblock[context.state.block_count].b_tail;
        context.state.cblock[context.state.block_count].length = F2T(context.state.cblock[context.state.block_count].f_end) - F2T(context.state.cblock[context.state.block_count].f_start);

        //If first cblock is < 1 sec. throw it away
        if( context.state.block_count > 0 ||
                F2L( context.state.cblock[context.state.block_count].f_end, context.state.cblock[context.state.block_count].f_start) > 1.0 ||
                context.state.cblock[context.state.block_count].f_end == context.state.framesprocessed
          )
        {

            BlocksDebug(context, 12, "blocks_create_cblock",
                                context.state.block_count, context.state.cblock[context.state.block_count].f_start, (context.state.cblock[context.state.block_count].f_start + context.state.cblock[context.state.block_count].b_head),
                                context.state.cblock[context.state.block_count].f_end, (context.state.cblock[context.state.block_count].f_end - context.state.cblock[context.state.block_count].b_tail),
                                CauseString(context, cause),
                                context.state.cblock[context.state.block_count].b_head, context.state.cblock[context.state.block_count].b_tail);

            comskip::detection::complete_block(context.state.cblock, context.state.block_count);
            prev_start = b_end + 1;							//cblock starts at end of black initially
            prev_head = b_end - b_start - b_counted + 1;	//remaining black from previous cblock tail
        }
    }


#if 1
    //Combine blocks with less than minimum black between them
    for (i = context.state.block_count-1; i >= 1; i--)
    {

        unsigned int bfcount = context.state.cblock[i].b_head + context.state.cblock[i-1].b_tail;

        if (bfcount < context.settings.min_black_frames_for_break && context.state.cblock[i-1].cause == comskip::detection::cause_value(comskip::detection::FrameCause::black))
        {

            BlocksDebug(context, 10, "blocks_combine_blocks",
                  i-1, i, context.state.cblock[i-1].f_end , bfcount);

            context.state.cblock[i-1].f_end	= context.state.cblock[i].f_end;
            context.state.cblock[i-1].b_tail	= context.state.cblock[i].b_tail;
            context.state.cblock[i-1].length	= F2L(context.state.cblock[i-1].f_end, context.state.cblock[i-1].f_start);
            context.state.cblock[i-1].cause	= context.state.cblock[i].cause;

            comskip::detection::erase_blocks(context.state.cblock, context.state.block_count, i);
        }
    }
#endif
    return (true);
}


void FindLogoThreshold(RecordingContext& context)
{
    constexpr std::size_t buckets = 20;
    if (context.state.framearray) {
        if (context.state.frame_count < 0)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
                comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
        const auto result = comskip::detection::build_logo_histogram(
            context.state.frame, static_cast<std::size_t>(context.state.frame_count), buckets);
        if (!result)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
                comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);
        if (std::ranges::any_of(result->counts, [](const auto count) {
                return count > static_cast<std::uint64_t>(std::numeric_limits<int>::max());
            }))
            throw comskip::diagnostics::DiagnosticError<std::overflow_error>(
                comskip::diagnostics::Code::invalid_detection_buffer_index_or_capacity);

        std::ranges::fill(context.state.logoHistogram, 0);
        for (std::size_t i = 0; i < result->counts.size(); ++i)
            context.state.logoHistogram[i] = static_cast<int>(result->counts[i]);

        OutputLogoHistogram(context, result->counts, result->denominator);
        context.state.logo_quality = result->quality;
        BlocksDebug(context, 8, "blocks_logo_quality",
                    std::format("{:.5f}", context.state.logo_quality));

        /*
                j = 0;
                for (i = 0; i < buckets/2; i++) {
                    j += logoHistogram[i];
                }
                k = 0;
                for (i = buckets/2; i < buckets; i++) {
                    k += logoHistogram[i];
                }
                if (k < j * 1.3) {
                    logo_quality = 0.9;
                } else {
                    k = logoHistogram[0];
                    for (i = 0; i < buckets; i++) {
                        if (logoHistogram[k] < logoHistogram[i]) {
                            k = i;
                        }
                    }
                    if (k < buckets * 2 / 3) {
                        logo_quality = 0.9;
                    } else {
                        i = 0;
                        j = logoHistogram[0];
                        for (i = buckets/2; i < k; i++) {
                            if (j * 10 / 8 >= logoHistogram[i]) {
                                j = logoHistogram[i];
                                logo_quality = ((double) i + 0.5) / (double) buckets;
                            }
                        }
                    }
                }
        */
    }
    if (context.settings.logo_threshold == 0)
    {
        context.settings.logo_threshold = context.state.logo_quality;
    }
}

void CleanLogoBlocks(RecordingContext& context)
{
    if (context.state.block_count == 0) return;
    int i,k,n;
//	double stdev;
    int sum_brightness,v,b, sum_volume,s,sum_silence,sum_uniform;
    double sum_brightness2;
    int sum_delta;
#if 1
    if ((context.settings.commDetectMethod & LOGO /* || startOverAfterLogoInfoAvail==0 */ ) &&! context.state.reverseLogoLogic && context.settings.connect_blocks_with_logo)
    {
        //Combine blocks with both logo
        for (i = context.state.block_count-1; i >= 1; i--)
        {
            if (CheckFrameForLogo(context, context.state.cblock[i-1].f_end) &&
                    CheckFrameForLogo(context, context.state.cblock[i].f_start) )
            {

                BlocksDebug(context, 6, "blocks_join_logo_blocks",
                      i-1, i, context.state.cblock[i-1].f_end);

                context.state.cblock[i-1].f_end	= context.state.cblock[i].f_end;
                context.state.cblock[i-1].b_tail	= context.state.cblock[i].b_tail;
                if (context.state.cblock[i].length > context.state.cblock[i-1].length)
                    context.state.cblock[i-1].ar_ratio = context.state.cblock[i].ar_ratio;	// Use AR of longest cblock
                context.state.cblock[i-1].length	= F2L(context.state.cblock[i-1].f_end, context.state.cblock[i-1].f_start);
                context.state.cblock[i-1].cause	= context.state.cblock[i].cause;

                comskip::detection::erase_blocks(context.state.cblock, context.state.block_count, i);
            }
        }
    }
#endif

    k = -1;
    //Checking cblock size ratio
    /*
        for (i = 0; i < block_count; i++) {

            if (F2L(cblock[i].f_end, cblock[i].f_start) > (int) min_show_segment_length )
            {
                if (k != -1 && i > k+1)
                {
                    a = cblock[k].f_end - cblock[k].f_start;
                    j = cblock[i].f_start - cblock[k].f_end;
                    Debug(1, "Long/Short cblock ratio for cblock %i till %i is %i percent\n",k, i-1 , (int)(100 * a)/(a+j));
                }
                k = i;
            }
        }
    */
    context.state.avg_brightness = 0;
    context.state.avg_volume = 0;
    context.state.maxi_volume = 0;
    context.state.avg_silence = 0;
    context.state.avg_uniform = 0;
    context.state.avg_schange = 0.0;
    for (i = 0; i < context.state.block_count; i++)
    {
        sum_brightness = 0;
        sum_volume = 0;
        sum_silence = 0;
        sum_uniform = 0;
        sum_brightness2 = 0.0;
        sum_delta = 0;
        if (context.state.framearray)
        {
            for (k = context.state.cblock[i].f_start+1; k < context.state.cblock[i].f_end; k++)
            {

//				b = frame[k].brightness;
                b = abs(context.state.frame[k].brightness - context.state.frame[k-1].brightness);
                v = context.state.frame[k].volume;
                if (context.state.maxi_volume < v)
                    context.state.maxi_volume = v;
                s = (context.state.frame[k].volume < context.settings.max_volume ? 0 : 99);
                sum_brightness += b;
                sum_volume += v;
                sum_silence += s;
                sum_uniform += abs(context.state.frame[k].uniform - context.state.frame[k-1].uniform);
                sum_brightness2 += b*b;
                sum_delta += abs(context.state.frame[k].brightness - context.state.frame[k-1].brightness);
            }
        }
        n = context.state.cblock[i].f_end - context.state.cblock[i].f_start+1;
        if (n>0) {
        context.state.cblock[i].brightness = sum_brightness * 1000 / n;
        context.state.cblock[i].volume = sum_volume / n;
        context.state.cblock[i].silence = sum_silence / n;
        context.state.cblock[i].uniform = sum_uniform / n;
        }
        if ((context.state.cblock[i].schange_count = CountSceneChanges(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end)))
        {
            context.state.cblock[i].schange_rate = (double)context.state.cblock[i].schange_count / n;
        }
        else
            context.state.cblock[i].schange_rate = 0.0;

        context.state.cblock[i].stdev =
//			sqrt( (n*sum_brightness2 - sum_brightness*sum_brightness)/ (n * (n-1)));
            100* sum_delta / n;
        context.state.avg_brightness += sum_brightness * 1000;
        context.state.avg_volume += sum_volume;
        context.state.avg_silence += sum_silence;
        context.state.avg_uniform += sum_uniform;
        context.state.avg_schange += context.state.cblock[i].schange_rate*n;
    }
    n = context.state.cblock[context.state.block_count-1].f_end - context.state.cblock[0].f_start;
    if (n>0) {
        context.state.avg_brightness /= n;
        context.state.avg_volume /= n;
        context.state.avg_silence /= n;
        context.state.avg_uniform /= n;
        context.state.avg_schange /= n;
    }
//	Debug(1, "Average brightness is %i\n",avg_brightness);
//	Debug(1, "Average volume is %i\n",avg_volume);

}

#define LOGO_BORDER 5
void InitScanLines(RecordingContext& context)
{
    int i;
    for (i = 0; i < context.state.height; i++)
    {
        if (i < context.state.clogoMinY - LOGO_BORDER || i > context.state.clogoMaxY + LOGO_BORDER)
        {
            context.state.lineStart[i] = context.settings.border;
            context.state.lineEnd[i] = context.state.videowidth-1-context.settings.border;
        }
        else
        {
            if ( context.state.clogoMinX > context.state.videowidth - context.state.clogoMaxX)   // Most pixels left of the logo
            {
                context.state.lineStart[i] = context.settings.border;
                context.state.lineEnd[i] = MAX(0,context.state.clogoMinX-LOGO_BORDER);
            }
            else
            {
                context.state.lineStart[i] = MIN(context.state.videowidth-1,context.state.clogoMaxX+LOGO_BORDER);
                context.state.lineEnd[i] = context.state.videowidth-1-context.settings.border;
            }
        }
    }
    for (i = context.state.height; i < MAXHEIGHT; i++)
    {
        context.state.lineStart[i] = 0;
        context.state.lineEnd[i] = 0;
    }
}

void InitHasLogo(RecordingContext& context)
{

    int x,y;
    context.state.ensure_pixel_buffers(comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo) != 0);
    std::ranges::fill(context.state.haslogo, 0);
    for (y = MAX(0,context.state.clogoMinY - LOGO_BORDER); y < MIN(context.state.height,context.state.clogoMaxY + LOGO_BORDER); y++)
    {
        for (x = MAX(0,context.state.clogoMinX-LOGO_BORDER); x < MIN(context.state.videowidth,context.state.clogoMaxX + LOGO_BORDER) ; x++)
        {
            context.state.haslogo[y*context.state.width+x] = 1;
        }
    }
}



