#include "app/debug.h"
#include "app/recording_context.h"
#include "block_building.h"
#include "block_features.h"
#include "captions.h"
#include "frame_causes.h"
#include "frame_timestamps.h"
#include "detection_methods.h"
#include "length_matching.h"
#include "logo_detection.h"
#include <algorithm>
#include <format>
#include <numeric>
#include <ranges>
#include <string_view>
#include <utility>

namespace {
template<class... Args>
void scoring_debug(RecordingContext& context, int level, std::string_view message_id, Args&&... args)
{
    // Debug discards messages above the verbosity level; skip translating them.
    if (context.settings.verbose < level) return;
    const auto message = context.translator.format(message_id, std::forward<Args>(args)...);
    Debug(context, level, message);
}
}

bool WithinDivisibleTolerance(double test_number, double divisor, double tolerance)
{
    double	added;
    double	remainder;
    added = test_number + tolerance;
    remainder = added - divisor * static_cast<int>(added / divisor);
    return ((remainder >= 0) && (remainder <= (2 * tolerance)));
}

// Match string ([*+]*[CS]+)*M([*+]*[CS]+)*

void WeighBlocks(RecordingContext& context)
{
    if (context.state.block_count == 0) return;
    int		i;
    int		j;
    int		k;
    double  cl;
    double	combined_length;
    double	tolerance;
    double  wscore = 0.0;
    double  lscore = 0.0;
    double	max_score = 99.99;
    int		max_combined_count = 25;
    bool	breakforcombine = false;

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::aspect_ratio))
    {
        SetARofBlocks(context);
    }


    for (i = 0; i < context.state.block_count-2; i++)
    {
        if (comskip::detection::cut_cause(context.state.cblock[i].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::aspect_ratio})  && comskip::detection::cut_cause(context.state.cblock[i+1].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::aspect_ratio})  &&
                context.state.cblock[i+1].length < 3.0 &&
                std::fabs(context.state.cblock[i].ar_ratio - context.state.cblock[i+2].ar_ratio) < context.settings.ar_delta
           )
        {
            scoring_debug(context, 2, "scoring_delete_short_same_ar", std::format("{}", i + 1),
                std::format("{}", context.state.cblock[i + 1].f_start));
            context.state.cblock[i].b_tail = context.state.cblock[i+2].b_tail;
            context.state.cblock[i].f_end = context.state.cblock[i+2].f_end;
            context.state.cblock[i].length += context.state.cblock[i+1].length + context.state.cblock[i+2].length;
            comskip::detection::erase_blocks(context.state.cblock, context.state.block_count, i + 1, 2);
        }
    }



    if (context.state.processCC)
    {
        PrintCCBlocks(context);
        for (i = 0; i < context.state.block_count; i++)
        {
            context.state.cblock[i].cc_type = DetermineCCTypeForBlock(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        }
    }


    if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo))
    {
        if (context.state.logoPercentage < context.settings.logo_fraction - 0.05 || context.state.logoPercentage > context.settings.logo_percentile)
        {
            scoring_debug(context, 1, "scoring_disable_logo_detection");
            comskip::detection::disable_method(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo);
            max_score = 10000;
        }
    }
    for (i = 0; i < context.state.block_count; i++)
    {
        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo))
        {
            context.state.cblock[i].logo = CalculateLogoFraction(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        }
        else
            context.state.cblock[i].logo = 0;
    }

    CleanLogoBlocks(context);		// Can join blocks, so recalculate logo

    if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::scene_change))
    {
        for (i = 0; i < context.state.block_count; i++)
        {
            scoring_debug(context, 5, "scoring_scene_change_rate", std::format("{:03}", i),
                std::format("{:.2f}", context.state.cblock[i].schange_rate), std::format("{:.2f}", context.state.avg_schange));
        }
    }

    for (i = 0; i < context.state.block_count; i++)
    {
        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo))
        {
            context.state.cblock[i].logo = CalculateLogoFraction(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        }
        else
            context.state.cblock[i].logo = 0;
    }


    if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo) && context.state.logoPercentage > 0.4)
    {
        if (context.settings.score_percentile + context.state.logoPercentage < 1.0)
            context.settings.score_percentile = context.state.logoPercentage + context.settings.score_percentile;
    }
    else if (context.settings.score_percentile < 0.5)
        context.settings.score_percentile = 0.71;

    scoring_debug(context, 5, "scoring_heading");



    for (i = 0; i < context.state.block_count; i++)
    {
        if (i == 0 || true )
        {
            j = i;
            combined_length = context.state.cblock[i].length;
            k = j;
            if (i > 0 && ((comskip::detection::cut_cause(context.state.cblock[i-1].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black})) || (comskip::detection::cut_cause(context.state.cblock[i-1].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::non_uniform}))))
                combined_length -= context.state.cblock[i].b_head / context.settings.fps / 4 ;

            if ((comskip::detection::cut_cause(context.state.cblock[i].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black})) || (comskip::detection::cut_cause(context.state.cblock[i].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::non_uniform})))
                combined_length -= context.state.cblock[j+1].b_head / context.settings.fps / 4 ;

            combined_length -= (context.state.cblock[i].b_head + context.state.cblock[j + 1].b_head) / context.settings.fps / 4;
            tolerance = (context.state.cblock[i].b_head + context.state.cblock[j + 1].b_head + 4) / context.settings.fps;
            if (IsStandardCommercialLength(context, combined_length, tolerance, true))
            {
                while (j>=i)
                {
                    context.state.cblock[j].strict = 2;
                    scoring_debug(context, 2, "scoring_strict_standard_length", std::format("{}", j));
                    scoring_debug(context, 3, "scoring_score_before", std::format("{}", j), std::format("{:.2f}", context.state.cblock[j].score));
                    context.state.cblock[j].score *= context.settings.length_strict_modifier;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[j].score));
                    context.state.cblock[j].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::strict);
                    context.state.cblock[j].more |= comskip::detection::cause_value(comskip::detection::BlockCause::strict);
                    j--;
                }
            }
            else if (IsStandardCommercialLength(context, combined_length, tolerance, false))
            {
                while (j>=i)
                {
                    context.state.cblock[j].strict = 1;
                    scoring_debug(context, 2, "scoring_nonstrict_standard_length", std::format("{}", j));
                    scoring_debug(context, 3, "scoring_score_before", std::format("{}", j), std::format("{:.2f}", context.state.cblock[j].score));
                    context.state.cblock[j].score *= context.settings.length_nonstrict_modifier;
                    context.state.cblock[j].score = (context.state.cblock[j].score > max_score) ? max_score : context.state.cblock[j].score;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[j].score));
                    context.state.cblock[j].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::non_strict);
                    context.state.cblock[j].more |= comskip::detection::cause_value(comskip::detection::BlockCause::non_strict);
                    j--;
                }
            }
            else
            {
                while (j>=i)
                {
                    context.state.cblock[j].strict = 0;
                    j--;
                }
            }
            j = k;
        }

        if (context.state.cblock[i].combined_count < max_combined_count)
        {
            combined_length = context.state.cblock[i].length;
            for (j = 1; j < context.state.block_count - i; j++)
            {
                // Evaluated for its optional strict-length training output.
                (void)IsStandardCommercialLength(context, context.state.cblock[i + j].length - (context.state.cblock[i+j].b_head + context.state.cblock[i + j + 1].b_head) / context.settings.fps, (context.state.cblock[i+j].bframe_count + context.state.cblock[i + j + 1].bframe_count + 2) / context.settings.fps, true);
                if ((context.state.cblock[i + j].combined_count > max_combined_count) || (context.state.cblock[i].combined_count > max_combined_count))
                {
                    scoring_debug(context, 3, "scoring_forward_combine_limit",
                        std::format("{}", i), std::format("{}", i + j), std::format("{}", i + j),
                        std::format("{}", context.state.cblock[i + j].combined_count));
                    breakforcombine = true;
                    break;
                }

                tolerance = (context.state.cblock[i].bframe_count + context.state.cblock[i + j + 1].bframe_count + 2) / context.settings.fps;
                combined_length += context.state.cblock[i + j].length;
                if (combined_length > (context.settings.max_commercial_size) + tolerance)
                {
                    break;
                }
                else
                {

                    if (IsStandardCommercialLength(context, combined_length - (context.state.cblock[i].b_head + context.state.cblock[i + j + 1].b_head) / context.settings.fps, tolerance, true) && context.settings.combined_length_strict_modifier != 1.0)
                    {
                        scoring_debug(context, 2, "scoring_combined_strict_length",
                            std::format("{}", i), std::format("{}", i + j), std::format("{:.2f}", combined_length),
                            std::format("{:f}", tolerance));
                        for (k = 0; k <= j; k++)
                        {
                            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i + k), std::format("{:.2f}", context.state.cblock[i + k].score));
                            context.state.cblock[i + k].score *= 1 + (context.settings.combined_length_strict_modifier / (j + 1) / 2);
                            context.state.cblock[i + k].score = (context.state.cblock[i + k].score > max_score) ? max_score : context.state.cblock[i + k].score;
                            context.state.cblock[i + k].combined_count += 1;
                            scoring_debug(context, 3, "scoring_score_after_combined", std::format("{:.2f}", context.state.cblock[i + k].score), std::format("{}", context.state.cblock[i + k].combined_count));
                            context.state.cblock[i + k].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);
                            context.state.cblock[i + k].more |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);

                        }
                    }
                    else if (IsStandardCommercialLength(context, combined_length - (context.state.cblock[i].b_head + context.state.cblock[i + j + 1].b_head) / context.settings.fps, tolerance, false) && context.settings.combined_length_nonstrict_modifier != 1.0)
                    {
                        scoring_debug(context, 2, "scoring_combined_nonstrict_length",
                            std::format("{}", i), std::format("{}", i + j), std::format("{:.2f}", combined_length),
                            std::format("{:f}", tolerance));
                        for (k = 0; k <= j; k++)
                        {
                            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i + k), std::format("{:.2f}", context.state.cblock[i + k].score));
                            context.state.cblock[i + k].score *= 1 + (context.settings.combined_length_nonstrict_modifier / (j + 1) / 2);
                            context.state.cblock[i + k].score = (context.state.cblock[i + k].score > max_score) ? max_score : context.state.cblock[i + k].score;
                            context.state.cblock[i + k].combined_count += 1;
                            scoring_debug(context, 3, "scoring_score_after_combined", std::format("{:.2f}", context.state.cblock[i + k].score), std::format("{}", context.state.cblock[i + k].combined_count));
                            context.state.cblock[i + k].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);
                            context.state.cblock[i + k].more |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);
                        }
                    }
                }
            }

            if (breakforcombine)
            {
                breakforcombine = false;
            }

            combined_length = context.state.cblock[i].length;
            for (j = 1; j < i; j++)
            {
                if (IsStandardCommercialLength(context, context.state.cblock[i - j].length - (context.state.cblock[i-j].b_head + context.state.cblock[i - j + 1].b_head)/context.settings.fps, (context.state.cblock[i-j].bframe_count + context.state.cblock[i - j + 1].bframe_count + 2) / context.settings.fps, true))
                {
                    break;
                }
                if ((context.state.cblock[i - j].combined_count > max_combined_count) || (context.state.cblock[i].combined_count > max_combined_count))
                {
                    scoring_debug(context, 3, "scoring_backward_combine_limit",
                        std::format("{}", i - j), std::format("{}", i), std::format("{}", i - j),
                        std::format("{}", context.state.cblock[i - j].combined_count));
                    breakforcombine = true;
                    break;
                }

                tolerance = (context.state.cblock[i + 1].bframe_count + context.state.cblock[i - j].bframe_count + 2) / context.settings.fps;
                combined_length += context.state.cblock[i - j].length;
                if (combined_length >= context.settings.max_commercial_size)
                {
                    break;
                }
                else
                {
                    if (IsStandardCommercialLength(context, combined_length - (context.state.cblock[i + 1].b_head + context.state.cblock[i - j].b_head) / context.settings.fps, tolerance, true) && context.settings.combined_length_strict_modifier != 1.0)
                    {
                        scoring_debug(context, 2, "scoring_combined_strict_length",
                            std::format("{}", i - j), std::format("{}", i), std::format("{:.2f}", combined_length),
                            std::format("{:f}", tolerance));
                        for (k = 0; k <= j; k++)
                        {
                            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i - k), std::format("{:.2f}", context.state.cblock[i - k].score));
                            context.state.cblock[i - k].score *= 1 + (context.settings.combined_length_strict_modifier / (j + 1) / 2);
                            context.state.cblock[i - k].score = (context.state.cblock[i - k].score > max_score) ? max_score : context.state.cblock[i - k].score;
                            context.state.cblock[i - k].combined_count += 1;
                            scoring_debug(context, 3, "scoring_score_after_combined", std::format("{:.2f}", context.state.cblock[i - k].score), std::format("{}", context.state.cblock[i - k].combined_count));
                            context.state.cblock[i - k].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);
                            context.state.cblock[i - k].more |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);
                        }
                    }
                    else if (IsStandardCommercialLength(context, combined_length - (context.state.cblock[i + 1].b_head + context.state.cblock[i - j].b_head) / context.settings.fps, tolerance, false) && context.settings.combined_length_nonstrict_modifier != 1.0)
                    {
                        scoring_debug(context, 2, "scoring_combined_nonstrict_length",
                            std::format("{}", i - j), std::format("{}", i), std::format("{:.2f}", combined_length),
                            std::format("{:f}", tolerance));
                        for (k = 0; k <= j; k++)
                        {
                            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i - k), std::format("{:.2f}", context.state.cblock[i - k].score));
                            context.state.cblock[i - k].score *= 1 + (context.settings.combined_length_nonstrict_modifier / (j + 1) / 2);
                            context.state.cblock[i - k].score = (context.state.cblock[i - k].score > max_score) ? max_score : context.state.cblock[i - k].score;
                            context.state.cblock[i - k].combined_count += 1;
                            scoring_debug(context, 3, "scoring_score_after_combined", std::format("{:.2f}", context.state.cblock[i - k].score), std::format("{}", context.state.cblock[i - k].combined_count));
                            context.state.cblock[i - k].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);
                            context.state.cblock[i - k].more |= comskip::detection::cause_value(comskip::detection::BlockCause::combined);

                        }
                    }
                }
            }

            if (breakforcombine)
            {
                breakforcombine = false;
            }
        }
        // if logo detected in cblock, score = 10%
        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo))
        {
            if (context.state.cblock[i].logo > context.settings.logo_percentage_threshold)
            {
                scoring_debug(context, 2, "scoring_block_has_logo", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.logo_present_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::logo);
                context.state.cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::logo);
            }
            else if (context.settings.punish_no_logo && context.state.cblock[i].logo < context.settings.logo_percentage_threshold && context.state.logoPercentage > context.settings.logo_fraction)
            {
                scoring_debug(context, 2, "scoring_block_has_no_logo", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= 2;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::logo);
                context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::logo);
            }
        }
        if ((context.settings.punish & 1) && context.state.cblock[i].brightness > context.state.avg_brightness * context.settings.punish_threshold)
        {
            scoring_debug(context, 2, "scoring_much_brighter", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.punish_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::above_brightness);
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::above_brightness);
        }
        if ((context.settings.punish & 2) && context.state.cblock[i].uniform > context.state.avg_uniform * context.settings.punish_threshold)
        {
            scoring_debug(context, 2, "scoring_less_uniform", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.punish_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::above_uniformity);
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::above_uniformity);
        }
        if ((context.settings.punish & 4) && context.state.cblock[i].volume > context.state.avg_volume * context.settings.punish_threshold)
        {
            scoring_debug(context, 2, "scoring_much_louder", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.punish_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::above_length);
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::above_length);
        }

        if ((context.settings.punish & 8) && context.state.cblock[i].silence > context.state.avg_silence * context.settings.punish_threshold)
        {
            scoring_debug(context, 2, "scoring_less_silence", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.punish_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::above_scene_change);
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::above_scene_change);
        }
        if ((context.settings.punish & 16) && context.state.cblock[i].schange_count > 2 && context.state.cblock[i].schange_rate > context.state.avg_schange * context.settings.punish_threshold)
        {
            scoring_debug(context, 2, "scoring_more_scene_change", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.punish_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::above_scene_change);
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::above_scene_change);
        }

        // if length > max_commercial_size * fps, score = 10%
        if (context.state.cblock[i].length > 2 * context.settings.min_show_segment_length)
        {
            scoring_debug(context, 2, "scoring_twice_excess_length", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.excessive_length_modifier * context.settings.excessive_length_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::exceeds);
            context.state.cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::exceeds);
        }
        else

            if (context.state.cblock[i].length > context.settings.min_show_segment_length)
            {
                scoring_debug(context, 2, "scoring_excess_length", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.excessive_length_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::exceeds);
                context.state.cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::exceeds);
            }

        // Mod score based on scene change rate
        // Mod score based on CC type
        if (context.state.processCC)
        {
        if (context.state.most_cc_type == comskip::detection::caption_type_value(comskip::detection::CaptionType::none))
            {
                if (context.state.cblock[i].cc_type != comskip::detection::caption_type_value(comskip::detection::CaptionType::none))
                {
                    scoring_debug(context, 3, "scoring_cc_in_non_cc_show_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score *= context.settings.cc_commercial_type_modifier * 2;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                }
            }
            else
            {
                if (context.state.cblock[i].cc_type == context.state.most_cc_type)
                {
                    scoring_debug(context, 3, "scoring_cc_correct_type_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score *= context.settings.cc_correct_type_modifier;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                }
                else if (context.state.cblock[i].cc_type == comskip::detection::caption_type_value(comskip::detection::CaptionType::commercial))
                {
                    scoring_debug(context, 3, "scoring_cc_commercial_type_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score *= context.settings.cc_commercial_type_modifier;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                }
                else if (context.state.cblock[i].cc_type == comskip::detection::caption_type_value(comskip::detection::CaptionType::none))
                {
                    scoring_debug(context, 3, "scoring_no_cc_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score *= (((context.settings.cc_wrong_type_modifier-1.0)/2)+1.0);
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                }
                else
                {
                    scoring_debug(context, 3, "scoring_cc_wrong_type_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score *= context.settings.cc_wrong_type_modifier;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                }
            }
        }

        // Mod score based on AR
        context.state.cblock[i].ar_ratio = AverageARForBlock(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        if ((context.state.dominant_ar - context.state.cblock[i].ar_ratio >= context.settings.ar_delta ||
                context.state.dominant_ar - context.state.cblock[i].ar_ratio <= - context.settings.ar_delta)
               )
        {
            scoring_debug(context, 2, "scoring_ar_differs", std::format("{}", i),
                std::format("{:.2f}", context.state.cblock[i].ar_ratio), std::format("{:.2f}", context.state.dominant_ar));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.ar_wrong_modifier;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::aspect_ratio);
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::aspect_ratio);
        }

                context.state.cblock[i].audio_channels = AverageACForBlock(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        if (context.state.dominant_ac != context.state.cblock[i].audio_channels)
        {
            scoring_debug(context, 2, "scoring_audio_channels_differ", std::format("{}", i),
                std::format("{}", context.state.cblock[i].audio_channels), std::format("{}", context.state.dominant_ac));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.ac_wrong_modifier;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::aspect_ratio);
            context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::aspect_ratio);
        }


    }

    if (context.state.processCC)
    {
        if (ProcessCCDict(context))
        {
            scoring_debug(context, 4, "scoring_dictionary_succeeded");
        }
        else
        {
            scoring_debug(context, 4, "scoring_dictionary_failed");
        }
    }

    if (!(context.settings.disable_heuristics & (1 << (2 - 1))))
    {
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( ((context.state.cblock[i].cause & comskip::detection::cause_value(comskip::detection::BlockCause::strict)) && (context.state.cblock[i].cause & (comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::non_uniform}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::silence}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::resolution_change}))) )  &&
                    context.state.cblock[i+1].score > 1.05 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i+2].score < 1.0  &&  context.state.cblock[i+2].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_after_strict", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
                context.state.cblock[i+1].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
            }
        }
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( ((context.state.cblock[i+2].cause & comskip::detection::cause_value(comskip::detection::BlockCause::strict)) && (context.state.cblock[i+1].cause & (comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::non_uniform}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::silence}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::resolution_change}))) )  &&
                    context.state.cblock[i+1].score > 1.05 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i].score < 1.0  &&  context.state.cblock[i].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_between_show_strict", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
                context.state.cblock[i+1].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
            }
        }

        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( (context.state.cblock[i].cause & (comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::non_uniform}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::resolution_change})))  && (context.state.cblock[i+1].cause & comskip::detection::frame_cause_mask({comskip::detection::FrameCause::aspect_ratio}))  &&
                    context.state.cblock[i+1].score > 1.0 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i+2].score < 1.0  &&  context.state.cblock[i+2].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_ar_after_commercial", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
                context.state.cblock[i+1].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
            }
        }
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( (context.state.cblock[i+1].cause & (comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::non_uniform}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::resolution_change})))  && (context.state.cblock[i].cause & comskip::detection::frame_cause_mask({comskip::detection::FrameCause::aspect_ratio}))  &&
                    context.state.cblock[i+1].score > 1.0 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i].score < 1.0  &&  context.state.cblock[i].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_ar_before_commercial", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
                context.state.cblock[i+1].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_2);
            }
        }

    }

    if (!(context.settings.disable_heuristics & (1 << (1 - 1))))
    {
        for (i = 0; i < context.state.block_count-1; i++)
        {
            if (context.state.cblock[i].score > 1.4 && context.state.cblock[i+1].score <= 1.05 &&  context.state.cblock[i+1].score > 0.0)
            {
                combined_length = 0;
                wscore = 0;
                lscore = 0;
                j = i+1;
                while (j < context.state.block_count && combined_length < context.settings.min_show_segment_length && context.state.cblock[j].score <= 1.05 && context.state.cblock[j].score > 0.0)
                {
                    combined_length += context.state.cblock[j].length;
                    wscore += context.state.cblock[j].length * context.state.cblock[j].score;
                    lscore += context.state.cblock[j].length * context.state.cblock[j].logo;
                    j++;
                }
                wscore /= combined_length;
                lscore /= combined_length;
                if (
                    ((combined_length < context.settings.min_show_segment_length / 2.0 && wscore > 0.9) ||
                     (combined_length < context.settings.min_show_segment_length / 3.0 && wscore > 0.3) ) &&
                    context.state.cblock[j].score > 1.4 &&
                    (combined_length < context.settings.min_show_segment_length / 6 ||
                     (context.state.cblock[i].f_start > context.state.after_start &&
                      context.state.cblock[j].f_end < context.state.before_end)))
                {
                    for (k = i+1; k < j; k++)
                    {
                        context.state.cblock[k].score = 99.99;
                        scoring_debug(context, 3, "scoring_h1_discard_between_strong", std::format("{}", k));
                        context.state.cblock[k].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_1);
                        context.state.cblock[k].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_1);
                    }

                }

            }
        }
        for (i = 0; i < context.state.block_count-1; i++)
        {
            if (context.state.cblock[i].score > 1.1 && context.state.cblock[i+1].score <= 1.05 &&  context.state.cblock[i+1].score > 0.0)
            {
                combined_length = 0.0;
                wscore = 0;
                lscore = 0;
                j = i+1;
                while (j < context.state.block_count && combined_length < context.settings.min_show_segment_length && context.state.cblock[j].score <= 1.05 && context.state.cblock[j].score > 0.0)
                {
                    combined_length += context.state.cblock[j].length;
                    wscore += context.state.cblock[j].length * context.state.cblock[j].score;
                    lscore += context.state.cblock[j].length * context.state.cblock[j].logo;
                    j++;
                }
                wscore /= combined_length;
                lscore /= combined_length;
                if (
                    ((combined_length < context.settings.min_show_segment_length / 4.0 && wscore > 0.9) ||
                     (combined_length < context.settings.min_show_segment_length / 6 && wscore > 0.3) ) &&
                    context.state.cblock[j].score > 1.1 &&
                    (combined_length < context.settings.min_show_segment_length / 12 ||
                     (context.state.cblock[i].f_start > context.state.after_start &&
                      context.state.cblock[j].f_end < context.state.before_end)))
                {
                    for (k = i+1; k < j; k++)
                    {
                        context.state.cblock[k].score = 99.99;
                        scoring_debug(context, 3, "scoring_h1_discard_between_weak", std::format("{}", k));
                        context.state.cblock[k].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_1);
                        context.state.cblock[k].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_1);
                    }

                }

            }
        }

    }

    if (!(context.settings.disable_heuristics & (1 << (8- 1))))
    {
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( (context.state.cblock[i].cause & (comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}) | comskip::detection::frame_cause_mask({comskip::detection::FrameCause::non_uniform}) ) )  &&
                    context.state.cblock[i].score > 1.05 &&
                    context.state.cblock[i].length < context.settings.min_show_segment_length &&
                    (i == 0 || context.state.cblock[i-1].score <1)
               )
            {
                k = j = context.state.cblock[i].f_end;
                while (j>1 && context.state.frame[j].brightness < 16)
                    j--;
                if (k - j > 10 &&
                    get_frame_pts(context, k) - get_frame_pts(context, j) > 5.0) // If more then 5 seconds dark frames
                {

                    context.state.cblock[i].score = 0.5;
                    scoring_debug(context, 3, "scoring_h8_add_dark_tail", std::format("{}", i));
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_8);
                    context.state.cblock[i].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_8);
                }
            }
        }
    }






    if (context.settings.delete_show_before_or_after_current && context.state.logo_block_count >= 80)
        scoring_debug(context, 10, "scoring_disable_logo_edge_processing");
    if (context.settings.delete_show_before_or_after_current &&
            (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo)) && context.settings.connect_blocks_with_logo &&
            !context.state.reverseLogoLogic && context.state.logoPercentage > context.settings.logo_fraction - 0.05 && context.state.logo_block_count < 40)
    {
        if (!(context.settings.disable_heuristics & (1 << (7 - 1))))
        {
            i = 0;
            while (i < context.state.block_count-1 && context.state.cblock[i].score < 1.05 &&
                    ((context.settings.delete_show_before_or_after_current == 1 && context.state.cblock[i].f_end < context.state.after_start) ||
                     (context.settings.delete_show_before_or_after_current > 1 && context.settings.delete_show_before_or_after_current > context.state.cblock[i].length)))
            {
                j = i+1;
                cl = 0;
                combined_length = 0;
                while (context.state.cblock[j].score > 1.05 && cl + context.state.cblock[j].length < context.settings.min_commercialbreak && j < context.state.block_count-1)
                {
                    combined_length += context.state.cblock[j].length;
                    cl += context.state.cblock[j].length;
                    j++;
                }
                if (context.state.cblock[j].score < 1.0 && context.state.cblock[j].length > context.settings.min_show_segment_length/2 )
                {
                    context.state.cblock[i].score = 99.99;
                    scoring_debug(context, 3, "scoring_h7_discard_logo_gap", std::format("{}", i),
                        std::format("{}", static_cast<int>(context.state.cblock[i].length)), std::format("{}", j));
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_7);
                    context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_7);
                    break;
                }
                i++;
            }

            i = context.state.block_count-1;
            while (i > 0 && context.state.cblock[i].score < 1.0 &&
                    ((context.settings.delete_show_before_or_after_current == 1 && context.state.cblock[i].f_start > context.state.before_end) ||
                     (context.settings.delete_show_before_or_after_current > 1 && context.settings.delete_show_before_or_after_current > context.state.cblock[i].length)))
            {
                j = i-1;
                cl = 0;
                combined_length = 0;
                while (context.state.cblock[j].score > 1.05 && cl + context.state.cblock[j].length < context.settings.min_commercialbreak && j >0)
                {
                    combined_length += context.state.cblock[j].length;
                    cl += context.state.cblock[j].length;
                    j--;
                }
                if (context.state.cblock[j].score < 1.0)
                {
                    context.state.cblock[i].score = 99.99;
                    scoring_debug(context, 3, "scoring_h7_discard_logo_gap", std::format("{}", i),
                        std::format("{}", static_cast<int>(context.state.cblock[i].length)), std::format("{}", j));
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_7);
                    context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_7);
                    break;
                }
                i++;
            }
        }
        if (!(context.settings.disable_heuristics & (1 << (3 - 1))))
        {
            for (i = 0; i < context.state.block_count-1; i++)
            {
                if (context.state.cblock[i].score < 1.0 && context.state.cblock[i].logo < 0.1 && context.state.cblock[i].length > context.settings.min_show_segment_length )
                {
                    if (context.state.cblock[i].f_end < context.state.after_start)
                    {
                        context.state.cblock[i].score *= 1.3;
                        scoring_debug(context, 3, "scoring_h3_demote_no_logo", std::format("{}", i), std::format("{}", i));
                        context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);
                        context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);

                    }
                    else if (context.state.cblock[i].f_start > context.state.before_end)
                    {
                        context.state.cblock[i].score *= 1.3;
                        scoring_debug(context, 3, "scoring_demote_no_logo", std::format("{}", i), std::format("{}", i));
                        context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);
                        context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);

                    }
                }
            }

        }
        for (i = 1; i < context.state.block_count-1; i++)
        {
            if (context.state.logoPercentage > context.settings.logo_fraction &&
                    context.state.cblock[i].score > 1.0 && context.state.cblock[i].logo < 0.1 &&
                    context.state.cblock[i].length > context.settings.min_show_segment_length+4 )
            {
                if (context.state.cblock[i].f_start > context.state.after_start &&
                        context.state.cblock[i].f_end   < context.state.before_end &&
                        (context.state.cblock[i-1].score < 1.0 || context.state.cblock[i+1].score < 1.0 ))
                {
                    context.state.cblock[i].score *= 0.5;
                    scoring_debug(context, 3, "scoring_promote_long_no_logo", std::format("{}", i), std::format("{}", i));
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);
                    context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);

                }
            }
        }

    }
    if (!(context.settings.disable_heuristics & (1 << (4 - 1))))
    {

        if ((comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo)) && !context.state.reverseLogoLogic && context.state.logoPercentage > context.settings.logo_fraction)
        {
            i = 1;
            while (i < context.state.block_count)
            {
                if (context.state.cblock[i].score < 1 && context.state.cblock[i].b_head > 7 && comskip::detection::cut_cause(context.state.cblock[i-1].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}))
                {
                    j = i-1;
                    k = 0;
                    while (j >= 0 && k < 5 && context.state.cblock[j].b_head > 7 && context.state.cblock[j].length < 7 && comskip::detection::cut_cause(context.state.cblock[j].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}))
                    {
                        context.state.cblock[j].score *= 0.1;   //  Add blocks with long black periods before show
                        scoring_debug(context, 3, "scoring_h4_add_black_gap", std::format("{}", j), std::format("{}", i));
                        k++;
                        context.state.cblock[j].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_4);
                        context.state.cblock[j].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_4);
                        j--;
                    }
                }
                i++;
            }
        }
        if ((comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo)) && !context.state.reverseLogoLogic && context.state.logoPercentage > context.settings.logo_fraction)
        {
            i = 0;
            while (i < context.state.block_count)
            {
                if (context.state.cblock[i].score < 1 && context.state.cblock[i].b_tail > 7 && comskip::detection::cut_cause(context.state.cblock[i].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}))
                {
                    j = i+1;
                    k = 0;
                    while (j < context.state.block_count && k < 5 && context.state.cblock[j].b_tail > 7 && context.state.cblock[j].length < 7 && comskip::detection::cut_cause(context.state.cblock[j-1].cause) == comskip::detection::frame_cause_mask({comskip::detection::FrameCause::black}))
                    {
                        context.state.cblock[j].score *= 0.1;   //  Add blocks with long black periods before show
                        scoring_debug(context, 3, "scoring_h4_add_black_gap", std::format("{}", j), std::format("{}", i));
                        k++;
                        context.state.cblock[j].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_4);
                        context.state.cblock[j].less |= comskip::detection::cause_value(comskip::detection::BlockCause::history_4);
                        j++;
                    }
                }
                i++;
            }
        }
    }
    if (context.settings.remove_silent_segments > 0 && !(context.settings.disable_heuristics & (1 << (9 - 1))))
    {
        for (i = 0; i < context.state.block_count; i++)
        {
            if (context.state.cblock[i].volume<20 && context.state.cblock[i].length > context.settings.remove_silent_segments )
            {
                   context.state.cblock[i].score = 5;
                    scoring_debug(context, 3, "scoring_h9_demote_silent", std::format("{}", i));
                    context.state.cblock[i].cause |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);
                    context.state.cblock[i].more |= comskip::detection::cause_value(comskip::detection::BlockCause::history_3);

            }
        }

    }



}

