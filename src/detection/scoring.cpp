#include "legacy_detection.h"
#include <format>
#include <string_view>
#include <utility>

namespace {
template<class... Args>
void scoring_debug(RecordingContext& context, int level, std::string_view message_id, Args&&... args)
{
    const auto message = context.translator.format(message_id, std::forward<Args>(args)...);
    Debug(context, level, "%s", message.c_str());
}
}

bool WithinDivisibleTolerance(double test_number, double divisor, double tolerance)
{
    double	added;
    double	remainder;
    added = test_number + tolerance;
    remainder = added - divisor * ((int)(added / (double)divisor));
    return ((remainder >= 0) && (remainder <= (2 * tolerance)));
}

/*
void CalculateCorrelation()
{
    int i,j;
    double position;
    double length;
    double min_weight;
    double pivot;
    double correlation;
    double distance;
    double weight;

    for (i = 0; i < block_count; i++) {
        pivot = ((double)(cblock[i].f_start+cblock[i].f_end)/(2*frame_count));
        length = ((double)(cblock[i].f_end - cblock[i].f_start)/frame_count);
        correlation = 0;
        min_weight = length;
        for (j = 0; j < block_count; j++) {
            if (i != j) {
                distance = fabs(pivot - ((double)(cblock[j].f_start+cblock[j].f_end)/(2*frame_count)));
                weight = ((double)(cblock[j].f_end - cblock[j].f_start)/frame_count);
                if (min_weight > weight)
                    min_weight = weight;
                correlation += (1 / (distance * distance) ) / (weight );
            }
        }
        cblock[i].correlation = correlation / length * (min_weight*min_weight*min_weight);
    }

}

void CalculateFit()
{
    int i,j;
    int start;
    double position;
    double length;
    double min_weight;
    double pivot;
    double correlation;
    double prev_correlation = 0;
    double distance;
    double weight;

    for (i = 0; i < block_count; i++) {
        start = cblock[i].f_start;
        correlation = 0;
        j = i+1;
        if (F2L(cblock[i].f_end, cblock[i].f_start) < max_commercial_size && j < block_count) {
            while ( F2L(cblock[j].f_end, cblock[j].f_start) < max_commercial_size && j < block_count - 1 && F2L(cblock[j].f_end, start)  < max_commercialbreak)
                j++;
            if ( F2L(cblock[j].f_start, start) > min_commercialbreak ) {
                correlation = j - i;
            }
        }
        if (correlation > prev_correlation)
            prev_correlation = correlation;
        cblock[i].correlation = prev_correlation;
        prev_correlation -= 1;
    }
}

*/

// Match string ([*+]*[CS]+)*M([*+]*[CS]+)*



/*
int MatchBlocks(int k, char *t)
{
    int match = false;
    char after[80];
    int i;
    int j;

    i = 0;
    while (t[i] != 0 && t[i] != 'M')
        i++;
    if (t[i] == 0)
        return(0);
    j = 0;
    while (t[i+j+1] != 0)
        after[j] = t[i+j+1];
        j++;
}
*/








void BuildPunish(RecordingContext& context)
{
    int i;
    int j;
    int t;
    int l;
    if (!context.state.length_sorted)
    {
        for (i=0 ; i< context.state.block_count; i++)
            context.state.length_order [i] = i;
again:
        for (j=0; j < context.state.block_count; j++)
        {
            for (i=j ; i< context.state.block_count; i++)
            {
                if (context.state.cblock[context.state.length_order[i]].length > context.state.cblock[context.state.length_order[j]].length)
                {
                    t = context.state.length_order[j];
                    context.state.length_order[j] = context.state.length_order[i];
                    context.state.length_order[i] = t;
                    goto again;
                }
            }
        }
        context.state.length_sorted = true;
    }
    context.state.max_val[0] = context.state.min_val[0] = context.state.cblock[context.state.length_order[0]].brightness;
    context.state.max_val[1] = context.state.min_val[1] = context.state.cblock[context.state.length_order[0]].volume;
    context.state.max_val[2] = context.state.min_val[2] = context.state.cblock[context.state.length_order[0]].silence;
    context.state.max_val[3] = context.state.min_val[3] = context.state.cblock[context.state.length_order[0]].uniform;
    context.state.max_val[4] = context.state.min_val[4] = context.state.cblock[context.state.length_order[0]].ar_ratio;
    context.state.max_val[5] = context.state.min_val[5] = context.state.cblock[context.state.length_order[0]].schange_rate;
    l = 0;
    for (i = 0; i < context.state.block_count; i++)
    {
        l += context.state.cblock[context.state.length_order[i]].length * context.settings.fps;
#define MINMAX(I,FIELD)	{	if (context.state.min_val[I] > context.state.cblock[context.state.length_order[i]].FIELD)			context.state.min_val[I] = context.state.cblock[context.state.length_order[i]].FIELD; 		if (context.state.max_val[I] < context.state.cblock[context.state.length_order[i]].FIELD) 			context.state.max_val[I] = context.state.cblock[context.state.length_order[i]].FIELD; }
        MINMAX(0, brightness)
        MINMAX(1, volume)
        MINMAX(2, silence)
        MINMAX(3, uniform)
        MINMAX(4, ar_ratio)
        MINMAX(5, schange_rate)
        if (l > context.state.cblock[context.state.block_count - 1].f_end* 70 / 100)
            break;
    }

}

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
    //bool	end_deleted = false;
    //bool	start_deleted = false;
    double	max_score = 99.99;
    int		max_combined_count = 25;
    bool	breakforcombine = false;

    if (context.settings.commDetectMethod & AR)
    {
//		showAvgAR = AverageARForBlock(1, framesprocessed);
        SetARofBlocks(context);
    }


    for (i = 0; i < context.state.block_count-2; i++)
    {
        if (CUTCAUSE(context.state.cblock[i].cause) == C_a  && CUTCAUSE(context.state.cblock[i+1].cause) == C_a  &&
                context.state.cblock[i+1].length < 3.0 &&
                fabs(context.state.cblock[i].ar_ratio - context.state.cblock[i+2].ar_ratio) < context.settings.ar_delta
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


    if (context.settings.commDetectMethod & LOGO)
    {
        if (context.state.logoPercentage < context.settings.logo_fraction - 0.05 || context.state.logoPercentage > context.settings.logo_percentile)
        {
            scoring_debug(context, 1, "scoring_disable_logo_detection");
            context.settings.commDetectMethod -= LOGO;
            max_score = 10000;
        }
    }
    for (i = 0; i < context.state.block_count; i++)
    {
        if (context.settings.commDetectMethod & LOGO)
        {
            context.state.cblock[i].logo = CalculateLogoFraction(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        }
        else
            context.state.cblock[i].logo = 0;
    }

//	CalculateCorrelation();
//	CalculateFit();

    CleanLogoBlocks(context);		// Can join blocks, so recalculate logo

    if (context.settings.commDetectMethod & SCENE_CHANGE)
    {
        for (i = 0; i < context.state.block_count; i++)
        {
            scoring_debug(context, 5, "scoring_scene_change_rate", std::format("{:03}", i),
                std::format("{:.2f}", context.state.cblock[i].schange_rate), std::format("{:.2f}", context.state.avg_schange));
        }
    }

    for (i = 0; i < context.state.block_count; i++)
    {
        if (context.settings.commDetectMethod & LOGO)
        {
            context.state.cblock[i].logo = CalculateLogoFraction(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        }
        else
            context.state.cblock[i].logo = 0;
    }


    if ((context.settings.commDetectMethod & LOGO) && context.state.logoPercentage > 0.4)
    {
        if (context.settings.score_percentile + context.state.logoPercentage < 1.0)
            context.settings.score_percentile = context.state.logoPercentage + context.settings.score_percentile;
    }
    else if (context.settings.score_percentile < 0.5)
        context.settings.score_percentile = 0.71;

//	if ((commDetectMethod & LOGO) && logoPercentage > logo_fraction && logoPercentage < logo_percentile && logo_present_modifier != 1.0)
//		excessive_length_modifier = 1;		// TESTING!!!!!!!!!!!!!!!!!!

    scoring_debug(context, 5, "scoring_heading");



    for (i = 0; i < context.state.block_count; i++)
    {
        if (i == 0 || true /*(cblock[i-1].cause & (C_b | C_u | C_v)) || cut_on_ar_change == 2 || 	(!(commDetectMethod & BLACK_FRAME) && (cblock[j].cause & C_v))  */)
        {
            j = i;
            combined_length = context.state.cblock[i].length;
//			while (j < block_count && ((cblock[j].cause & C_a) && (cut_on_ar_change == 1)  && !	(!(commDetectMethod & BLACK_FRAME) && (cblock[j].cause & C_v))  ) ) {
//				j++;
//				combined_length += cblock[j].length;
//			}
//expand:
            k = j;
            if (i > 0 && ((CUTCAUSE(context.state.cblock[i-1].cause) == C_b) || (CUTCAUSE(context.state.cblock[i-1].cause) == C_u)))
                combined_length -= context.state.cblock[i].b_head / context.settings.fps / 4 ;

            if ((CUTCAUSE(context.state.cblock[i].cause) == C_b) || (CUTCAUSE(context.state.cblock[i].cause) == C_u))
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
//					cblock[j].score *= length_strict_modifier;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[j].score));
                    context.state.cblock[j].cause |= C_STRICT;
                    context.state.cblock[j].more |= C_STRICT;
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
                    context.state.cblock[j].cause |= C_NONSTRICT;
                    context.state.cblock[j].more |= C_NONSTRICT;
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
//			if (j+1 < block_count && cblock[i].strict == 0 && cblock[j+1].length < 5.0) {
//				j++;
//				combined_length += cblock[j].length;
//				goto expand;
//			}
        }
        /*
                tolerance = (cblock[i].bframe_count + cblock[i + 1].bframe_count + 6) / fps;
                if (IsStandardCommercialLength(cblock[i].length, tolerance, true)) {
                    cblock[i].strict = 2;
                    Debug(2, "Block %i has strict standard length for a commercial.\n", i);
                    Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                    cblock[i].score *= length_strict_modifier;
                    cblock[i].score *= length_strict_modifier;
                    Debug(3, "After - %.2f\n", cblock[i].score);
                    cblock[i].cause |= C_STRICT;
                } else if (IsStandardCommercialLength(cblock[i].length, tolerance, false)) {
                    cblock[i].strict = 1;
                    Debug(2, "Block %i has non-strict standard length for a commercial.\n", i);
                    Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                    cblock[i].score *= length_nonstrict_modifier;
                    cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                    Debug(3, "After - %.2f\n", cblock[i].score);
                    cblock[i].cause |= C_NONSTRICT;
                } else
                    cblock[i].strict = 0;
        */

#if 1
        if (context.state.cblock[i].combined_count < max_combined_count)
        {
//			Debug(3, "Attempting to combine cblock %i\n", i);
            combined_length = context.state.cblock[i].length;
            for (j = 1; j < context.state.block_count - i; j++)
            {
                if (IsStandardCommercialLength(context, context.state.cblock[i + j].length - (context.state.cblock[i+j].b_head + context.state.cblock[i + j + 1].b_head) / context.settings.fps,
                                               (context.state.cblock[i+j].bframe_count + context.state.cblock[i + j + 1].bframe_count + 2) / context.settings.fps, true))
                {
                    /*					Debug(
                                            3,
                                            "Not attempting to forward combine blocks %i to %i because cblock %i is strict commercial.\n",
                                            i,
                                            i + j,
                                            i + j
                                        );
                    */
//					break;
                }
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
//					Debug(2, "Not trying to combine blocks %i thru %i due to excessive length - %f\n", i, i + j, combined_length);
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
                            context.state.cblock[i + k].cause |= C_COMBINED;
                            context.state.cblock[i + k].more |= C_COMBINED;

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
                            context.state.cblock[i + k].cause |= C_COMBINED;
                            context.state.cblock[i + k].more |= C_COMBINED;
                        }
                    }
                }
            }

            if (breakforcombine)
            {
//				Debug(3, "Block %i Break for forward combined limit\n", i);
                breakforcombine = false;
            }

            combined_length = context.state.cblock[i].length;
            for (j = 1; j < i; j++)
            {
                if (IsStandardCommercialLength(context, context.state.cblock[i - j].length - (context.state.cblock[i-j].b_head + context.state.cblock[i - j + 1].b_head)/context.settings.fps, (context.state.cblock[i-j].bframe_count + context.state.cblock[i - j + 1].bframe_count + 2) / context.settings.fps, true))
                {
                    /*					Debug(
                                            3,
                                            "Not attempting to forward combine blocks %i to %i because cblock %i is strict commercial.\n",
                                            i - j,
                                            i,
                                            i - j
                                        );
                    */
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
//					Debug(2, "Not trying to backward combine blocks %i thru %i due to excessive length - %f\n", i - j, i, combined_length);
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
                            context.state.cblock[i - k].cause |= C_COMBINED;
                            context.state.cblock[i - k].more |= C_COMBINED;
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
                            context.state.cblock[i - k].cause |= C_COMBINED;
                            context.state.cblock[i - k].more |= C_COMBINED;

                        }
                    }
                }
            }

            if (breakforcombine)
            {
//				Debug(3, "Block %i Break for backward combined limit\n", i);
                breakforcombine = false;
            }
        }
#endif
        // if logo detected in cblock, score = 10%
        if (context.settings.commDetectMethod & LOGO)
        {
            if (context.state.cblock[i].logo > context.settings.logo_percentage_threshold)
            {
                scoring_debug(context, 2, "scoring_block_has_logo", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.logo_present_modifier;
//				cblock[i].score *= (logo_present_modifier*cblock[i].logo) + (1-cblock[i].logo);
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_LOGO;
                context.state.cblock[i].less |= C_LOGO;
            }
            /*			else if (cblock[i].logo > 0.10) {
                            Debug(2, "Block %i has logo.\n", i);
                            Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                            cblock[i].score *= logo_present_modifier;
            //				cblock[i].score *= (logo_present_modifier*cblock[i].logo) + (1-cblock[i].logo);
                            cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                            Debug(3, "After - %.2f\n", cblock[i].score);
                            cblock[i].cause |= C_LOGO;
                            cblock[i].less |= C_LOGO;
                        }
            */			else if (context.settings.punish_no_logo && context.state.cblock[i].logo < context.settings.logo_percentage_threshold && context.state.logoPercentage > context.settings.logo_fraction)
            {
                scoring_debug(context, 2, "scoring_block_has_no_logo", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= 2;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_LOGO;
                context.state.cblock[i].more |= C_LOGO;
            }
        }
        BuildPunish(context);
        if (true)
        {
            if ((context.settings.punish & 1) && context.state.cblock[i].brightness > context.state.avg_brightness * context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_much_brighter", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.punish_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_AB;
                context.state.cblock[i].more |= C_AB;
            }
            if ((context.settings.punish & 2) && context.state.cblock[i].uniform > context.state.avg_uniform * context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_less_uniform", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.punish_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_AU;
                context.state.cblock[i].more |= C_AU;
            }
            if ((context.settings.punish & 4) && context.state.cblock[i].volume > context.state.avg_volume * context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_much_louder", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.punish_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_AL;
                context.state.cblock[i].more |= C_AL;
            }

            if ((context.settings.punish & 8) && context.state.cblock[i].silence > context.state.avg_silence * context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_less_silence", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.punish_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_AS;
                context.state.cblock[i].more |= C_AS;
            }
            if ((context.settings.punish & 16) && context.state.cblock[i].schange_count > 2 && context.state.cblock[i].schange_rate > context.state.avg_schange * context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_more_scene_change", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.punish_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_AC;
                context.state.cblock[i].more |= C_AC;
            }
        }
        if (false)
        {
            if ((context.settings.reward & 1) && context.state.cblock[i].brightness < context.state.avg_brightness / context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_much_darker", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.reward_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_BRIGHT;
                context.state.cblock[i].less |= C_BRIGHT;
            }
            if ((context.settings.reward & 2) && context.state.cblock[i].uniform < context.state.avg_uniform / context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_more_uniform", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.reward_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_BRIGHT;
                context.state.cblock[i].less |= C_BRIGHT;
            }
            if ((context.settings.reward & 4) && context.state.cblock[i].volume < context.state.avg_volume / context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_much_quieter", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.reward_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_BRIGHT;
                context.state.cblock[i].less |= C_BRIGHT;
            }
            if ((context.settings.reward & 8) && context.state.cblock[i].silence < context.state.avg_silence / context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_more_silence", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.reward_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_BRIGHT;
                context.state.cblock[i].less |= C_BRIGHT;
            }
            if ((context.settings.reward & 16) && context.state.cblock[i].schange_count > 2 && context.state.cblock[i].schange_rate < context.state.avg_schange / context.settings.punish_threshold)
            {
                scoring_debug(context, 2, "scoring_less_scene_change", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.reward_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_BRIGHT;
                context.state.cblock[i].less |= C_BRIGHT;
            }
        }

//		cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > min_show_segment_length
#if 0
        // if length < min_show_segment_length, score = 150%
        if (context.state.cblock[i].length < min_show_segment_length && context.state.cblock[i].logo < 0.2 ))
        {
            scoring_debug(context, 2, "scoring_shorter_than_minimum_show_segment", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= 1.5;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
        }

#endif
#if 0
        if (framearray && context.state.cblock[i].length < max_commercialbreak &&
                    context.state.cblock[i].brightness < avg_brightness)
        {
            scoring_debug(context, 2, "scoring_short_low_brightness", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= dark_block_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
        }
#endif
        // if length > max_commercial_size * fps, score = 10%
        if (context.state.cblock[i].length > 2 * context.settings.min_show_segment_length)
        {
            scoring_debug(context, 2, "scoring_twice_excess_length", std::format("{}", i));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.excessive_length_modifier * context.settings.excessive_length_modifier;
            context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= C_EXCEEDS;
            context.state.cblock[i].less |= C_EXCEEDS;
        }
        else

            if (context.state.cblock[i].length > context.settings.min_show_segment_length)
            {
                scoring_debug(context, 2, "scoring_excess_length", std::format("{}", i));
                scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].score *= context.settings.excessive_length_modifier;
                context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                context.state.cblock[i].cause |= C_EXCEEDS;
                context.state.cblock[i].less |= C_EXCEEDS;
            }

        // Mod score based on scene change rate
        /*
                if ( (commDetectMethod & SCENE_CHANGE) && (cblock[i].schange_count > 2) && (cblock[i].length > 3)) {
        #if 0
                    schange_modifier = (cblock[i].schange_rate / avg_schange);
                    schange_modifier = (schange_modifier > min_schange_modifier) ? schange_modifier : min_schange_modifier;
                    schange_modifier = (schange_modifier < max_schange_modifier) ? schange_modifier : max_schange_modifier;
                    Debug(3, "SC modifier - %.3f\tBlock %i score:\tBefore - %.2f\t", schange_modifier, i, cblock[i].score);
                    cblock[i].score *= schange_modifier;
                    cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                    Debug(3, "\tSC\tAfter - %.2f\n", cblock[i].score);
        #else
                    schange_modifier = (cblock[i].schange_rate / avg_schange);
                    if (schange_modifier > 2.0 || schange_modifier < 0.5  ) {
                        schange_modifier = (schange_modifier > min_schange_modifier) ? schange_modifier : min_schange_modifier;
                        schange_modifier = (schange_modifier < max_schange_modifier) ? schange_modifier : max_schange_modifier;
                        Debug(3, "SC modifier - %.3f\tBlock %i score:\tBefore - %.2f\t", schange_modifier, i, cblock[i].score);
                        cblock[i].score *= schange_modifier;
                        cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                        Debug(3, "\tSC\tAfter - %.2f\n", cblock[i].score);
                        cblock[i].cause |= C_SC;
                    }
        #endif

                }
        */
        // Mod score based on CC type
        if (context.state.processCC)
        {
        if (context.state.most_cc_type == NONE)
            {
                if (context.state.cblock[i].cc_type != NONE)
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
                else if (context.state.cblock[i].cc_type == COMMERCIAL)
                {
                    scoring_debug(context, 3, "scoring_cc_commercial_type_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score *= context.settings.cc_commercial_type_modifier;
                    scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
                    context.state.cblock[i].score = (context.state.cblock[i].score > max_score) ? max_score : context.state.cblock[i].score;
                }
                else if (context.state.cblock[i].cc_type == NONE)
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
//		if (commDetectMethod & AR) {
        context.state.cblock[i].ar_ratio = AverageARForBlock(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        if ((context.state.dominant_ar - context.state.cblock[i].ar_ratio >= context.settings.ar_delta ||
                context.state.dominant_ar - context.state.cblock[i].ar_ratio <= - context.settings.ar_delta)
//				cblock[i].length < min_show_segment_length
//				&& (cblock[i].length > 5.0 || cblock[i].ar_ratio - ar_delta < dominant_ar)
               )
        {
            scoring_debug(context, 2, "scoring_ar_differs", std::format("{}", i),
                std::format("{:.2f}", context.state.cblock[i].ar_ratio), std::format("{:.2f}", context.state.dominant_ar));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.ar_wrong_modifier;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= C_AR;
            context.state.cblock[i].more |= C_AR;
        }
        //		}

                context.state.cblock[i].audio_channels = AverageACForBlock(context, context.state.cblock[i].f_start, context.state.cblock[i].f_end);
        if (context.state.dominant_ac != context.state.cblock[i].audio_channels)
        {
            scoring_debug(context, 2, "scoring_audio_channels_differ", std::format("{}", i),
                std::format("{}", context.state.cblock[i].audio_channels), std::format("{}", context.state.dominant_ac));
            scoring_debug(context, 3, "scoring_score_before", std::format("{}", i), std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].score *= context.settings.ac_wrong_modifier;
            scoring_debug(context, 3, "scoring_score_after", std::format("{:.2f}", context.state.cblock[i].score));
            context.state.cblock[i].cause |= C_AR;
            context.state.cblock[i].more |= C_AR;
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
    for (i = 0; i < context.state.block_count; i++)
    {
//		OutputStrict(cblock[i].length, (double) cblock[i].strict, 0.0);
    }


    if (!(context.settings.disable_heuristics & (1 << (2 - 1))))
    {
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( ((context.state.cblock[i].cause & C_STRICT) && (context.state.cblock[i].cause & (C_b | C_u | C_v | C_r)) )  &&
                    context.state.cblock[i+1].score > 1.05 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i+2].score < 1.0  &&  context.state.cblock[i+2].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_after_strict", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= C_H2;
                context.state.cblock[i+1].less |= C_H2;
            }
        }
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( ((context.state.cblock[i+2].cause & C_STRICT) && (context.state.cblock[i+1].cause & (C_b | C_u | C_v | C_r)) )  &&
                    context.state.cblock[i+1].score > 1.05 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i].score < 1.0  &&  context.state.cblock[i].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_between_show_strict", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= C_H2;
                context.state.cblock[i+1].less |= C_H2;
            }
        }

        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( (context.state.cblock[i].cause & (C_b | C_u | C_r))  && (context.state.cblock[i+1].cause & C_a)  &&
                    context.state.cblock[i+1].score > 1.0 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i+2].score < 1.0  &&  context.state.cblock[i+2].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_ar_after_commercial", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= C_H2;
                context.state.cblock[i+1].less |= C_H2;
            }
        }
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( (context.state.cblock[i+1].cause & (C_b | C_u | C_r))  && (context.state.cblock[i].cause & C_a)  &&
                    context.state.cblock[i+1].score > 1.0 &&  context.state.cblock[i+1].length < 4.8 &&
                    context.state.cblock[i].score < 1.0  &&  context.state.cblock[i].length > context.settings.min_show_segment_length
               )
            {
                context.state.cblock[i+1].score = 0.5;
                scoring_debug(context, 3, "scoring_h2_add_ar_before_commercial", std::format("{}", i + 1));
                context.state.cblock[i+1].cause |= C_H2;
                context.state.cblock[i+1].less |= C_H2;
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
                if (//lscore < 0.36 &&
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
                        context.state.cblock[k].cause |= C_H1;
                        context.state.cblock[k].more |= C_H1;
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
                if (//lscore < 0.36 &&
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
                        context.state.cblock[k].cause |= C_H1;
                        context.state.cblock[k].more |= C_H1;
                    }

                }

            }
        }

    }


    /*
    for (i = 0; i < block_count-2; i++) {
        if (cblock[i].score < 0.9 && cblock[i+1].score == 1.0 && cblock[i+1].length < min_show_segment_length/2 && cblock[i+2].score > 1.5) {
            cblock[i+1].score *= 1.5;
            Debug(3, "Discarding cblock %i because short and on edge between commercial and show.\n",
                    i+1);
        }
        if (cblock[i].score > 1.5 && cblock[i+1].score == 1.0 && cblock[i+1].length < min_show_segment_length/2 && cblock[i+2].score < 0.9) {
            cblock[i+1].score *= 1.5;
            Debug(3, "Discarding cblock %i because short and on edge between commercial and show.\n",
                    i+1);
        }
    }
    */

    /*
        if (delete_show_before_or_after_current && logoPercentage == 0) {
            i = 0;
            while (i < block_count-1 && cblock[i].score < 1.0 && cblock[i].f_end < before_end) {
                j = i+1;
                cl = 0.0;
                while (cblock[j].score > 1.05 && cl + cblock[j].length < min_commercialbreak && j < block_count-1) {
                    cl += cblock[j].length;
                    j++;
                }
                if (cblock[j].score < 1.0) {
                    cblock[i].score = 99.99;
                    Debug(3, "Discarding cblock %i because separated from cblock %i with small non show gap.\n",
                        i, j);
                    cblock[i].cause |= C_H2;
                    cblock[i].more |= C_H2;
                    start_deleted = true;
                    break;
                }
                i++;
            }
            if (! start_deleted) {
                j = 0;
                i = 0;
                if (cblock[j].score < 1.0) {
                    cblock[i].score = 99.99;
                    Debug(3, "Discarding cblock %i because of being first block.\n",
                        i, j);
                    cblock[i].cause |= C_H2;
                    cblock[i].more |= C_H2;
                    start_deleted = true;
                }
            }
            i = block_count-1;
            while (i > 0 && cblock[i].score < 1.05 && cblock[i].f_start > before_end) {
                j = i-1;
                cl = 0;
                while (cblock[j].score > 1.05 && cl + cblock[j].length < min_commercialbreak && j >0) {
                    cl += cblock[j].length;
                    j--;
                }
                if (cblock[j].score < 1.0) {
                    cblock[i].score = 99.99;
                    Debug(3, "Discarding cblock %i because seprated from cblock %i with small non show gap.\n",
                        i, j);
                    cblock[i].cause |= C_H2;
                    cblock[i].more |= C_H2;
                    end_deleted = true;
                    break;
                }
                i++;
            }

            if (! end_deleted) {
                i = block_count-1;
                j = block_count-1;
                if (cblock[j].score < 1.05) {
                    cblock[i].score = 99.99;
                    Debug(3, "Discarding cblock %i because being last block.\n",
                        i, j);
                    cblock[i].cause |= C_H2;
                    cblock[i].more |= C_H2;
                    end_deleted = true;
                }
            }
        }
    */

    if (!(context.settings.disable_heuristics & (1 << (8- 1))))
    {
        for (i = 0; i < context.state.block_count-2; i++)
        {
            if ( (context.state.cblock[i].cause & (C_b | C_u ) )  &&
                    context.state.cblock[i].score > 1.05 &&
                    context.state.cblock[i].length < context.settings.min_show_segment_length &&
                    (i == 0 || context.state.cblock[i-1].score <1)
               )
            {
                k = j = context.state.cblock[i].f_end;
                while (j>1 && context.state.frame[j].brightness < 16)
                    j--;
                if (k - j > 10 &&
                    F2T(k) - F2T(j) > 5.0) // If more then 5 seconds dark frames
                {

                    context.state.cblock[i].score = 0.5;
                    scoring_debug(context, 3, "scoring_h8_add_dark_tail", std::format("{}", i));
                    context.state.cblock[i].cause |= C_H8;
                    context.state.cblock[i].less |= C_H8;
                }
            }
        }
    }






    if (context.settings.delete_show_before_or_after_current && context.state.logo_block_count >= 80)
        scoring_debug(context, 10, "scoring_disable_logo_edge_processing");
    if (context.settings.delete_show_before_or_after_current &&
            (context.settings.commDetectMethod & LOGO) && context.settings.connect_blocks_with_logo &&
            !context.state.reverseLogoLogic && context.state.logoPercentage > context.settings.logo_fraction - 0.05 && context.state.logo_block_count < 40)
    {
        /*
                for (i = 0; i < block_count-1; i++) {
                    if (cblock[i].score < 1.0 && cblock[i].logo > 0.2 && cblock[i+1].score < 1.0 && cblock[i+1].logo > 0.2 ) {
                        if (cblock[i].f_end < after_start) {
                            cblock[i].score = 99.99;
                            Debug(3, "Discarding cblock %i because cblock %i has also logo.\n",
                                i, i+1);
                        } else if (cblock[i+1].f_start > before_end) {
                            cblock[i+1].score = 99.99;
                            Debug(3, "Discarding cblock %i because cblock %i has also logo.\n",
                                i+1, i);
                        }
                    }
                }
            */
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
                    context.state.cblock[i].cause |= C_H7;
                    context.state.cblock[i].more |= C_H7;
                    //start_deleted = true;
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
                    context.state.cblock[i].cause |= C_H7;
                    context.state.cblock[i].more |= C_H7;
                    //end_deleted = true;
                    break;
                }
                i++;
            }
        }
        /*
                for (i = 0; i < block_count-1; i++) {
                    if (cblock[i].score < 1.0

        //				&& cblock[i+1].score > 1.05 && cblock[i+1].length < 10 && cblock[i+2].score < 1.0 && cblock[i+2].logo > 0.2
                        ) {
                        j = i+1;
                        cl = 0;
                        while (cblock[j].score > 1.05 && cl + cblock[j].length < min_commercialbreak && j < block_count-1) {
                            cl += cblock[j].length;
                            j++;
                        }
                        if (cblock[j].score < 1.0) {
                            if (cblock[j].f_start < after_start) {
                                cblock[i].score = 99.99;
                                Debug(3, "Discarding cblock %i because cblock %i has also logo and small non logo gap.\n",
                                    i, j);
                            } else if (cblock[i].f_end > before_end) {
                                cblock[j].score = 99.99;
                                Debug(3, "Discarding cblock %i because cblock %i has also logo and small non logo gap.\n",
                                    j, i);
                            }
                        }
                    }
                }
        */
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
                        context.state.cblock[i].cause |= C_H3;
                        context.state.cblock[i].more |= C_H3;

                    }
                    else if (context.state.cblock[i].f_start > context.state.before_end)
                    {
                        context.state.cblock[i].score *= 1.3;
                        scoring_debug(context, 3, "scoring_demote_no_logo", std::format("{}", i), std::format("{}", i));
                        context.state.cblock[i].cause |= C_H3;
                        context.state.cblock[i].more |= C_H3;

                    }
                }
            }

        }
//	if (!(disable_heuristics & (1 << (3 - 1)))) {
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
                    context.state.cblock[i].cause |= C_H3;
                    context.state.cblock[i].more |= C_H3;

                }
            }
        }

//	}
    }
    if (!(context.settings.disable_heuristics & (1 << (4 - 1))))
    {

        if ((context.settings.commDetectMethod & LOGO) && !context.state.reverseLogoLogic && context.state.logoPercentage > context.settings.logo_fraction)
        {
            i = 1;
            while (i < context.state.block_count)
            {
                if (context.state.cblock[i].score < 1 && context.state.cblock[i].b_head > 7 && CUTCAUSE(context.state.cblock[i-1].cause) == C_b)
                {
                    j = i-1;
                    k = 0;
                    while (j >= 0 && k < 5 && context.state.cblock[j].b_head > 7 && context.state.cblock[j].length < 7 && CUTCAUSE(context.state.cblock[j].cause) == C_b)
                    {
                        context.state.cblock[j].score *= 0.1;   //  Add blocks with long black periods before show
                        scoring_debug(context, 3, "scoring_h4_add_black_gap", std::format("{}", j), std::format("{}", i));
                        k++;
                        context.state.cblock[j].cause |= C_H4;
                        context.state.cblock[j].less |= C_H4;
                        j--;
                    }
                }
                i++;
            }
        }
        if ((context.settings.commDetectMethod & LOGO) && !context.state.reverseLogoLogic && context.state.logoPercentage > context.settings.logo_fraction)
        {
            i = 0;
            while (i < context.state.block_count)
            {
                if (context.state.cblock[i].score < 1 && context.state.cblock[i].b_tail > 7 && CUTCAUSE(context.state.cblock[i].cause) == C_b)
                {
                    j = i+1;
                    k = 0;
                    while (j < context.state.block_count && k < 5 && context.state.cblock[j].b_tail > 7 && context.state.cblock[j].length < 7 && CUTCAUSE(context.state.cblock[j-1].cause) == C_b)
                    {
                        context.state.cblock[j].score *= 0.1;   //  Add blocks with long black periods before show
                        scoring_debug(context, 3, "scoring_h4_add_black_gap", std::format("{}", j), std::format("{}", i));
                        k++;
                        context.state.cblock[j].cause |= C_H4;
                        context.state.cblock[j].less |= C_H4;
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
                    context.state.cblock[i].cause |= C_H3;
                    context.state.cblock[i].more |= C_H3;

            }
        }

    }



    if (!(context.settings.disable_heuristics & (1 << (2 - 1))))
    {
        /*		i = 0;
                cl = 0;
                while (cblock[i].score < 1.05 && cl + cblock[i].length < min_commercialbreak && i < block_count-1) {
                    k += cblock[i].length;
                    i++;
                }
                if (i < block_count-1 && cblock[i].score > 1.05 && cl < min_commercialbreak) {
                    for (j = 0; j < i; j++) {
                        cblock[j].score = 99.99;
                        Debug(3, "H2 Discarding cblock %i because too short and before commercial.\n",
                            j);
                        cblock[j].cause |= C_H2;
                        cblock[j].more |= C_H2;
                    }
                }
        */
    }
    /*

        if (!(disable_heuristics & (1 << (1 - 1)))) {

        for (i = 0; i < block_count-2; i++) {
            if (cblock[i].score < 1.05 && cblock[i+1].score > 1.05 && cblock[i+2].score < 1.05 &&
                cblock[i+1].length > min_show_segment_length && logoPercentage < 0.7) {
                j = i + 2;
                for (k = i+1; k < j; k++) {
                        cblock[k].score = 0.05;
                        Debug(3, "H1 Included cblock %i because too long and between two show blocks.\n",
                            k);
                        cblock[k].cause |= C_H1;
                        cblock[k].more |= C_H1;
                }

            }
        }
        }
    */
}
/*
    for (i = 0; i < block_count-2; i++) {
        if (cblock[i].score < 0.9 && cblock[i+1].score > 1.0 && cblock[i+2].score > 1.5 &&
            !(cblock[i].cause & (C_b | C_u | C_v | C_a)) ) {
            cblock[i+1].score = 0.5;
            Debug(3, "Eroded cblock %i because vague cut reason and on edge between commercial and show.\n",
                    i+1);
        }
        if (cblock[i].score > 1.5 && cblock[i+1].score > 1.0 && cblock[i+2].score < 0.9 &&
            !(cblock[i+1].cause & (C_b | C_u | C_v | C_a)) ) {
            cblock[i+1].score = 0.5;
            Debug(3, "Eroded cblock %i because vague cut reason and on edge between commercial and show.\n",
                    i+1);
        }
    }
*/

