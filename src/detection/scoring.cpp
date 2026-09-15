#include "legacy_detection.h"

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

int beforeblocks[100];
int afterblocks[100];
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

int length_order[2000];
int length_sorted = false;
int min_val[10];
int max_val[10];
int delta_val[10];


void BuildPunish()
{
    int i;
    int j;
    int t;
    int l;
    if (!length_sorted)
    {
        for (i=0 ; i< block_count; i++)
            length_order [i] = i;
again:
        for (j=0; j < block_count; j++)
        {
            for (i=j ; i< block_count; i++)
            {
                if (cblock[length_order[i]].length > cblock[length_order[j]].length)
                {
                    t = length_order[j];
                    length_order[j] = length_order[i];
                    length_order[i] = t;
                    goto again;
                }
            }
        }
        length_sorted = true;
    }
    max_val[0] = min_val[0] = cblock[length_order[0]].brightness;
    max_val[1] = min_val[1] = cblock[length_order[0]].volume;
    max_val[2] = min_val[2] = cblock[length_order[0]].silence;
    max_val[3] = min_val[3] = cblock[length_order[0]].uniform;
    max_val[4] = min_val[4] = cblock[length_order[0]].ar_ratio;
    max_val[5] = min_val[5] = cblock[length_order[0]].schange_rate;
    l = 0;
    for (i = 0; i < block_count; i++)
    {
        l += cblock[length_order[i]].length * fps;
#define MINMAX(I,FIELD)	{	if (min_val[I] > cblock[length_order[i]].FIELD)			min_val[I] = cblock[length_order[i]].FIELD; 		if (max_val[I] < cblock[length_order[i]].FIELD) 			max_val[I] = cblock[length_order[i]].FIELD; }
        MINMAX(0, brightness)
        MINMAX(1, volume)
        MINMAX(2, silence)
        MINMAX(3, uniform)
        MINMAX(4, ar_ratio)
        MINMAX(5, schange_rate)
        if (l > cblock[block_count - 1].f_end* 70 / 100)
            break;
    }

}

void WeighBlocks(void)
{
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

    if (commDetectMethod & AR)
    {
//		showAvgAR = AverageARForBlock(1, framesprocessed);
        SetARofBlocks();
    }


    for (i = 0; i < block_count-2; i++)
    {
        if (CUTCAUSE(cblock[i].cause) == C_a  && CUTCAUSE(cblock[i+1].cause) == C_a  &&
                cblock[i+1].length < 3.0 &&
                fabs(cblock[i].ar_ratio - cblock[i+2].ar_ratio) < ar_delta
           )
        {
            Debug(2, "Deleting cblock %d starting at frame %d because too short and same AR before and after\n", i+1, cblock[i+1].f_start);
            cblock[i].b_tail = cblock[i+2].b_tail;
            cblock[i].f_end = cblock[i+2].f_end;
            cblock[i].length += cblock[i+1].length + cblock[i+2].length;
            for (j = i+1; j < block_count-2; j++)
            {
                cblock[j] = cblock[j+2];
            }
            block_count = block_count - 2;
        }
    }



    if (processCC)
    {
        PrintCCBlocks();
        for (i = 0; i < block_count; i++)
        {
            cblock[i].cc_type = DetermineCCTypeForBlock(cblock[i].f_start, cblock[i].f_end);
        }
    }


    if (commDetectMethod & LOGO)
    {
        if (logoPercentage < logo_fraction - 0.05 || logoPercentage > logo_percentile)
        {
            Debug(1, "Not enough or too much logo's found, disabling the use of Logo detection\n", i);
            commDetectMethod -= LOGO;
            max_score = 10000;
        }
    }
    for (i = 0; i < block_count; i++)
    {
        if (commDetectMethod & LOGO)
        {
            cblock[i].logo = CalculateLogoFraction(cblock[i].f_start, cblock[i].f_end);
        }
        else
            cblock[i].logo = 0;
    }

//	CalculateCorrelation();
//	CalculateFit();

    CleanLogoBlocks();		// Can join blocks, so recalculate logo

    if (commDetectMethod & SCENE_CHANGE)
    {
        for (i = 0; i < block_count; i++)
        {
            Debug(5, "Block %.3i\tschange_rate - %.2f\t average - %.2f\n", i, cblock[i].schange_rate, avg_schange);
        }
    }

    for (i = 0; i < block_count; i++)
    {
        if (commDetectMethod & LOGO)
        {
            cblock[i].logo = CalculateLogoFraction(cblock[i].f_start, cblock[i].f_end);
        }
        else
            cblock[i].logo = 0;
    }


    if ((commDetectMethod & LOGO) && logoPercentage > 0.4)
    {
        if (score_percentile + logoPercentage < 1.0)
            score_percentile = logoPercentage + score_percentile;
    }
    else if (score_percentile < 0.5)
        score_percentile = 0.71;

//	if ((commDetectMethod & LOGO) && logoPercentage > logo_fraction && logoPercentage < logo_percentile && logo_present_modifier != 1.0)
//		excessive_length_modifier = 1;		// TESTING!!!!!!!!!!!!!!!!!!

    Debug(5, "\nFuzzy scoring of the blocks\n---------------------------\n");



    for (i = 0; i < block_count; i++)
    {
        if (i == 0 || true /*(cblock[i-1].cause & (C_b | C_u | C_v)) || cut_on_ar_change == 2 || 	(!(commDetectMethod & BLACK_FRAME) && (cblock[j].cause & C_v))  */)
        {
            j = i;
            combined_length = cblock[i].length;
//			while (j < block_count && ((cblock[j].cause & C_a) && (cut_on_ar_change == 1)  && !	(!(commDetectMethod & BLACK_FRAME) && (cblock[j].cause & C_v))  ) ) {
//				j++;
//				combined_length += cblock[j].length;
//			}
//expand:
            k = j;
            if (i > 0 && ((CUTCAUSE(cblock[i-1].cause) == C_b) || (CUTCAUSE(cblock[i-1].cause) == C_u)))
                combined_length -= cblock[i].b_head / fps / 4 ;

            if ((CUTCAUSE(cblock[i].cause) == C_b) || (CUTCAUSE(cblock[i].cause) == C_u))
                combined_length -= cblock[j+1].b_head / fps / 4 ;

            combined_length -= (cblock[i].b_head + cblock[j + 1].b_head) / fps / 4;
            tolerance = (cblock[i].b_head + cblock[j + 1].b_head + 4) / fps;
            if (IsStandardCommercialLength(combined_length, tolerance, true))
            {
                while (j>=i)
                {
                    cblock[j].strict = 2;
                    Debug(2, "Block %i has strict standard length for a commercial.\n", j);
                    Debug(3, "Block %i score:\tBefore - %.2f\t", j, cblock[j].score);
                    cblock[j].score *= length_strict_modifier;
//					cblock[j].score *= length_strict_modifier;
                    Debug(3, "After - %.2f\n", cblock[j].score);
                    cblock[j].cause |= C_STRICT;
                    cblock[j].more |= C_STRICT;
                    j--;
                }
            }
            else if (IsStandardCommercialLength(combined_length, tolerance, false))
            {
                while (j>=i)
                {
                    cblock[j].strict = 1;
                    Debug(2, "Block %i has non-strict standard length for a commercial.\n", j);
                    Debug(3, "Block %i score:\tBefore - %.2f\t", j, cblock[j].score);
                    cblock[j].score *= length_nonstrict_modifier;
                    cblock[j].score = (cblock[j].score > max_score) ? max_score : cblock[j].score;
                    Debug(3, "After - %.2f\n", cblock[j].score);
                    cblock[j].cause |= C_NONSTRICT;
                    cblock[j].more |= C_NONSTRICT;
                    j--;
                }
            }
            else
            {
                while (j>=i)
                {
                    cblock[j].strict = 0;
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
        if (cblock[i].combined_count < max_combined_count)
        {
//			Debug(3, "Attempting to combine cblock %i\n", i);
            combined_length = cblock[i].length;
            for (j = 1; j < block_count - i; j++)
            {
                if (IsStandardCommercialLength(cblock[i + j].length - (cblock[i+j].b_head + cblock[i + j + 1].b_head) / fps,
                                               (cblock[i+j].bframe_count + cblock[i + j + 1].bframe_count + 2) / fps, true))
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
                if ((cblock[i + j].combined_count > max_combined_count) || (cblock[i].combined_count > max_combined_count))
                {
                    Debug(
                        3,
                        "Not attempting to forward combine blocks %i to %i because cblock %i has already been combined %i times.\n",
                        i,
                        i + j,
                        i + j,
                        cblock[i + j].combined_count
                    );
                    breakforcombine = true;
                    break;
                }

                tolerance = (cblock[i].bframe_count + cblock[i + j + 1].bframe_count + 2) / fps;
                combined_length += cblock[i + j].length;
                if (combined_length > (max_commercial_size) + tolerance)
                {
//					Debug(2, "Not trying to combine blocks %i thru %i due to excessive length - %f\n", i, i + j, combined_length);
                    break;
                }
                else
                {

                    if (IsStandardCommercialLength(combined_length - (cblock[i].b_head + cblock[i + j + 1].b_head) / fps, tolerance, true) && combined_length_strict_modifier != 1.0)
                    {
                        Debug(
                            2,
                            "Combining Blocks %i thru %i result in strict standard commercial length of %.2f with a tolerance of %f.\n",
                            i,
                            i + j,
                            combined_length,
                            tolerance
                        );
                        for (k = 0; k <= j; k++)
                        {
                            Debug(3, "Block %i score:\tBefore - %.2f\t", i + k, cblock[i + k].score);
                            cblock[i + k].score *= 1 + (combined_length_strict_modifier / (j + 1) / 2);
                            cblock[i + k].score = (cblock[i + k].score > max_score) ? max_score : cblock[i + k].score;
                            cblock[i + k].combined_count += 1;
                            Debug(3, "After - %.2f\tCombined count - %i\n", cblock[i + k].score, cblock[i + k].combined_count);
                            cblock[i + k].cause |= C_COMBINED;
                            cblock[i + k].more |= C_COMBINED;

                        }
                    }
                    else if (IsStandardCommercialLength(combined_length - (cblock[i].b_head + cblock[i + j + 1].b_head) / fps, tolerance, false) && combined_length_nonstrict_modifier != 1.0)
                    {
                        Debug(
                            2,
                            "Combining Blocks %i thru %i result in non-strict standard commercial length of %.2f with a tolerance of %f.\n",
                            i,
                            i + j,
                            combined_length,
                            tolerance
                        );
                        for (k = 0; k <= j; k++)
                        {
                            Debug(3, "Block %i score:\tBefore - %.2f\t", i + k, cblock[i + k].score);
                            cblock[i + k].score *= 1 + (combined_length_nonstrict_modifier / (j + 1) / 2);
                            cblock[i + k].score = (cblock[i + k].score > max_score) ? max_score : cblock[i + k].score;
                            cblock[i + k].combined_count += 1;
                            Debug(3, "After - %.2f\tCombined count - %i\n", cblock[i + k].score, cblock[i + k].combined_count);
                            cblock[i + k].cause |= C_COMBINED;
                            cblock[i + k].more |= C_COMBINED;
                        }
                    }
                }
            }

            if (breakforcombine)
            {
//				Debug(3, "Block %i Break for forward combined limit\n", i);
                breakforcombine = false;
            }

            combined_length = cblock[i].length;
            for (j = 1; j < i; j++)
            {
                if (IsStandardCommercialLength(cblock[i - j].length - (cblock[i-j].b_head + cblock[i - j + 1].b_head)/fps, (cblock[i-j].bframe_count + cblock[i - j + 1].bframe_count + 2) / fps, true))
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
                if ((cblock[i - j].combined_count > max_combined_count) || (cblock[i].combined_count > max_combined_count))
                {
                    Debug(
                        3,
                        "Not attempting to backward combine blocks %i to %i because cblock %i has already been combined %i times.\n",
                        i - j,
                        i,
                        i - j,
                        cblock[i - j].combined_count
                    );
                    breakforcombine = true;
                    break;
                }

                tolerance = (cblock[i + 1].bframe_count + cblock[i - j].bframe_count + 2) / fps;
                combined_length += cblock[i - j].length;
                if (combined_length >= max_commercial_size)
                {
//					Debug(2, "Not trying to backward combine blocks %i thru %i due to excessive length - %f\n", i - j, i, combined_length);
                    break;
                }
                else
                {
                    if (IsStandardCommercialLength(combined_length - (cblock[i + 1].b_head + cblock[i - j].b_head) / fps, tolerance, true) && combined_length_strict_modifier != 1.0)
                    {
                        Debug(
                            2,
                            "Combining Blocks %i thru %i result in strict standard commercial length of %.2f with a tolerance of %f.\n",
                            i - j,
                            i,
                            combined_length,
                            tolerance
                        );
                        for (k = 0; k <= j; k++)
                        {
                            Debug(3, "Block %i score:\tBefore - %.2f\t", i - k, cblock[i - k].score);
                            cblock[i - k].score *= 1 + (combined_length_strict_modifier / (j + 1) / 2);
                            cblock[i - k].score = (cblock[i - k].score > max_score) ? max_score : cblock[i - k].score;
                            cblock[i - k].combined_count += 1;
                            Debug(3, "After - %.2f\tCombined count - %i\n", cblock[i - k].score, cblock[i - k].combined_count);
                            cblock[i - k].cause |= C_COMBINED;
                            cblock[i - k].more |= C_COMBINED;
                        }
                    }
                    else if (IsStandardCommercialLength(combined_length - (cblock[i + 1].b_head + cblock[i - j].b_head) / fps, tolerance, false) && combined_length_nonstrict_modifier != 1.0)
                    {
                        Debug(
                            2,
                            "Combining Blocks %i thru %i result in non-strict standard commercial length of %.2f with a tolerance of %f.\n",
                            i - j,
                            i,
                            combined_length,
                            tolerance
                        );
                        for (k = 0; k <= j; k++)
                        {
                            Debug(3, "Block %i score:\tBefore - %.2f\t", i - k, cblock[i - k].score);
                            cblock[i - k].score *= 1 + (combined_length_nonstrict_modifier / (j + 1) / 2);
                            cblock[i - k].score = (cblock[i - k].score > max_score) ? max_score : cblock[i - k].score;
                            cblock[i - k].combined_count += 1;
                            Debug(3, "After - %.2f\tCombined count - %i\n", cblock[i - k].score, cblock[i - k].combined_count);
                            cblock[i - k].cause |= C_COMBINED;
                            cblock[i - k].more |= C_COMBINED;

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
        if (commDetectMethod & LOGO)
        {
            if (cblock[i].logo > logo_percentage_threshold)
            {
                Debug(2, "Block %i has logo.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= logo_present_modifier;
//				cblock[i].score *= (logo_present_modifier*cblock[i].logo) + (1-cblock[i].logo);
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_LOGO;
                cblock[i].less |= C_LOGO;
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
            */			else if (punish_no_logo && cblock[i].logo < logo_percentage_threshold && logoPercentage > logo_fraction)
            {
                Debug(2, "Block %i has no logo.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= 2;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_LOGO;
                cblock[i].more |= C_LOGO;
            }
        }
        BuildPunish();
        if (true)
        {
            if ((punish & 1) && cblock[i].brightness > avg_brightness * punish_threshold)
            {
                Debug(2, "Block %i is much brighter than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= punish_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_AB;
                cblock[i].more |= C_AB;
            }
            if ((punish & 2) && cblock[i].uniform > avg_uniform * punish_threshold)
            {
                Debug(2, "Block %i is less uniform than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= punish_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_AU;
                cblock[i].more |= C_AU;
            }
            if ((punish & 4) && cblock[i].volume > avg_volume * punish_threshold)
            {
                Debug(2, "Block %i is much louder than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= punish_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_AL;
                cblock[i].more |= C_AL;
            }

            if ((punish & 8) && cblock[i].silence > avg_silence * punish_threshold)
            {
                Debug(2, "Block %i has less silence than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= punish_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_AS;
                cblock[i].more |= C_AS;
            }
            if ((punish & 16) && cblock[i].schange_count > 2 && cblock[i].schange_rate > avg_schange * punish_threshold)
            {
                Debug(2, "Block %i has more scene change than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= punish_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_AC;
                cblock[i].more |= C_AC;
            }
        }
        if (false)
        {
            if ((reward & 1) && cblock[i].brightness < avg_brightness / punish_threshold)
            {
                Debug(2, "Block %i is much darker than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= reward_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_BRIGHT;
                cblock[i].less |= C_BRIGHT;
            }
            if ((reward & 2) && cblock[i].uniform < avg_uniform / punish_threshold)
            {
                Debug(2, "Block %i is more uniform than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= reward_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_BRIGHT;
                cblock[i].less |= C_BRIGHT;
            }
            if ((reward & 4) && cblock[i].volume < avg_volume / punish_threshold)
            {
                Debug(2, "Block %i is much quieter than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= reward_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_BRIGHT;
                cblock[i].less |= C_BRIGHT;
            }
            if ((reward & 8) && cblock[i].silence < avg_silence / punish_threshold)
            {
                Debug(2, "Block %i has more silence than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= reward_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_BRIGHT;
                cblock[i].less |= C_BRIGHT;
            }
            if ((reward & 16) && cblock[i].schange_count > 2 && cblock[i].schange_rate < avg_schange / punish_threshold)
            {
                Debug(2, "Block %i has less scene change than average.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= reward_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_BRIGHT;
                cblock[i].less |= C_BRIGHT;
            }
        }

//		cblock[i].logo > 0.5 && F2L(cblock[i].f_end, cblock[i].f_start) > min_show_segment_length
#if 0
        // if length < min_show_segment_length, score = 150%
        if (cblock[i].length < min_show_segment_length && cblock[i].logo < 0.2 ))
        {
            Debug(2, "Block %i is shorter then minimum show segment.\n", i);
            Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
            cblock[i].score *= 1.5;
            cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
            Debug(3, "After - %.2f\n", cblock[i].score);
        }

#endif
#if 0
        if (framearray && cblock[i].length < max_commercialbreak &&
                    cblock[i].brightness < avg_brightness)
        {
            Debug(2, "Block %i is short but has low brightness.\n", i);
            Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
            cblock[i].score *= dark_block_modifier;
            cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
            Debug(3, "After - %.2f\n", cblock[i].score);
        }
#endif
        // if length > max_commercial_size * fps, score = 10%
        if (cblock[i].length > 2 * min_show_segment_length)
        {
            Debug(2, "Block %i has twice excess length.\n", i);
            Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
            cblock[i].score *= excessive_length_modifier * excessive_length_modifier;
            cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
            Debug(3, "After - %.2f\n", cblock[i].score);
            cblock[i].cause |= C_EXCEEDS;
            cblock[i].less |= C_EXCEEDS;
        }
        else

            if (cblock[i].length > min_show_segment_length)
            {
                Debug(2, "Block %i has excess length.\n", i);
                Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                cblock[i].score *= excessive_length_modifier;
                cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                Debug(3, "After - %.2f\n", cblock[i].score);
                cblock[i].cause |= C_EXCEEDS;
                cblock[i].less |= C_EXCEEDS;
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
        if (processCC)
        {
        if (most_cc_type == NONE)
            {
                if (cblock[i].cc_type != NONE)
                {
                    Debug(3, "CC's exist in a non-CC'd show - Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                    cblock[i].score *= cc_commercial_type_modifier * 2;
                    Debug(3, "After - %.2f\n", cblock[i].score);
                    cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                }
            }
            else
            {
                if (cblock[i].cc_type == most_cc_type)
                {
                    Debug(3, "CC's correct type - Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                    cblock[i].score *= cc_correct_type_modifier;
                    Debug(3, "After - %.2f\n", cblock[i].score);
                    cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                }
                else if (cblock[i].cc_type == COMMERCIAL)
                {
                    Debug(3, "CC's commercial type - Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                    cblock[i].score *= cc_commercial_type_modifier;
                    Debug(3, "After - %.2f\n", cblock[i].score);
                    cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                }
                else if (cblock[i].cc_type == NONE)
                {
                    Debug(3, "No CC's - Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                    cblock[i].score *= (((cc_wrong_type_modifier-1.0)/2)+1.0);
                    Debug(3, "After - %.2f\n", cblock[i].score);
                    cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                }
                else
                {
                    Debug(3, "CC's wrong type - Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
                    cblock[i].score *= cc_wrong_type_modifier;
                    Debug(3, "After - %.2f\n", cblock[i].score);
                    cblock[i].score = (cblock[i].score > max_score) ? max_score : cblock[i].score;
                }
            }
        }

        // Mod score based on AR
//		if (commDetectMethod & AR) {
        cblock[i].ar_ratio = AverageARForBlock(cblock[i].f_start, cblock[i].f_end);
        if ((dominant_ar - cblock[i].ar_ratio >= ar_delta ||
                dominant_ar - cblock[i].ar_ratio <= - ar_delta)
//				cblock[i].length < min_show_segment_length
//				&& (cblock[i].length > 5.0 || cblock[i].ar_ratio - ar_delta < dominant_ar)
               )
        {
            Debug(2, "Block %i AR (%.2f) is different from dominant AR(%.2f).\n",i,cblock[i].ar_ratio, dominant_ar);
            Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
            cblock[i].score *= ar_wrong_modifier;
            Debug(3, "After - %.2f\n", cblock[i].score);
            cblock[i].cause |= C_AR;
            cblock[i].more |= C_AR;
        }
        //		}

                cblock[i].audio_channels = AverageACForBlock(cblock[i].f_start, cblock[i].f_end);
        if (dominant_ac != cblock[i].audio_channels)
        {
            Debug(2, "Block %i audio_channels (%i) is different from dominant audio_channels (%i).\n",i,cblock[i].audio_channels, dominant_ac);
            Debug(3, "Block %i score:\tBefore - %.2f\t", i, cblock[i].score);
            cblock[i].score *= ac_wrong_modifier;
            Debug(3, "After - %.2f\n", cblock[i].score);
            cblock[i].cause |= C_AR;
            cblock[i].more |= C_AR;
        }


    }

    if (processCC)
    {
        if (ProcessCCDict())
        {
            Debug(4, "Dictionary processed successfully\n");
        }
        else
        {
            Debug(4, "Dictionary not processed successfully\n");
        }
    }
    for (i = 0; i < block_count; i++)
    {
//		OutputStrict(cblock[i].length, (double) cblock[i].strict, 0.0);
    }


    if (!(disable_heuristics & (1 << (2 - 1))))
    {
        for (i = 0; i < block_count-2; i++)
        {
            if ( ((cblock[i].cause & C_STRICT) && (cblock[i].cause & (C_b | C_u | C_v | C_r)) )  &&
                    cblock[i+1].score > 1.05 &&  cblock[i+1].length < 4.8 &&
                    cblock[i+2].score < 1.0  &&  cblock[i+2].length > min_show_segment_length
               )
            {
                cblock[i+1].score = 0.5;
                Debug(3, "H2 Added cblock %i because short and after strict commercial.\n", i+1);
                cblock[i+1].cause |= C_H2;
                cblock[i+1].less |= C_H2;
            }
        }
        for (i = 0; i < block_count-2; i++)
        {
            if ( ((cblock[i+2].cause & C_STRICT) && (cblock[i+1].cause & (C_b | C_u | C_v | C_r)) )  &&
                    cblock[i+1].score > 1.05 &&  cblock[i+1].length < 4.8 &&
                    cblock[i].score < 1.0  &&  cblock[i].length > min_show_segment_length
               )
            {
                cblock[i+1].score = 0.5;
                Debug(3, "H2 Added cblock %i because after show, short and before strict commercial.\n", i+1);
                cblock[i+1].cause |= C_H2;
                cblock[i+1].less |= C_H2;
            }
        }

        for (i = 0; i < block_count-2; i++)
        {
            if ( (cblock[i].cause & (C_b | C_u | C_r))  && (cblock[i+1].cause & C_a)  &&
                    cblock[i+1].score > 1.0 &&  cblock[i+1].length < 4.8 &&
                    cblock[i+2].score < 1.0  &&  cblock[i+2].length > min_show_segment_length
               )
            {
                cblock[i+1].score = 0.5;
                Debug(3, "H2 Added cblock %i because short and based on aspect ratio change after commercial.\n", i+1);
                cblock[i+1].cause |= C_H2;
                cblock[i+1].less |= C_H2;
            }
        }
        for (i = 0; i < block_count-2; i++)
        {
            if ( (cblock[i+1].cause & (C_b | C_u | C_r))  && (cblock[i].cause & C_a)  &&
                    cblock[i+1].score > 1.0 &&  cblock[i+1].length < 4.8 &&
                    cblock[i].score < 1.0  &&  cblock[i].length > min_show_segment_length
               )
            {
                cblock[i+1].score = 0.5;
                Debug(3, "H2 Added cblock %i because short and based on aspect ratio change before commercial.\n", i+1);
                cblock[i+1].cause |= C_H2;
                cblock[i+1].less |= C_H2;
            }
        }

    }

    if (!(disable_heuristics & (1 << (1 - 1))))
    {
        for (i = 0; i < block_count-1; i++)
        {
            if (cblock[i].score > 1.4 && cblock[i+1].score <= 1.05 &&  cblock[i+1].score > 0.0)
            {
                combined_length = 0;
                wscore = 0;
                lscore = 0;
                j = i+1;
                while (j < block_count && combined_length < min_show_segment_length && cblock[j].score <= 1.05 && cblock[j].score > 0.0)
                {
                    combined_length += cblock[j].length;
                    wscore += cblock[j].length * cblock[j].score;
                    lscore += cblock[j].length * cblock[j].logo;
                    j++;
                }
                wscore /= combined_length;
                lscore /= combined_length;
                if (//lscore < 0.36 &&
                    ((combined_length < min_show_segment_length / 2.0 && wscore > 0.9) ||
                     (combined_length < min_show_segment_length / 3.0 && wscore > 0.3) ) &&
                    cblock[j].score > 1.4 &&
                    (combined_length < min_show_segment_length / 6 ||
                     (cblock[i].f_start > after_start &&
                      cblock[j].f_end < before_end)))
                {
                    for (k = i+1; k < j; k++)
                    {
                        cblock[k].score = 99.99;
                        Debug(3, "H1 Discarding cblock %i because too short and between two strong commercial blocks.\n",
                              k);
                        cblock[k].cause |= C_H1;
                        cblock[k].more |= C_H1;
                    }

                }

            }
        }
        for (i = 0; i < block_count-1; i++)
        {
            if (cblock[i].score > 1.1 && cblock[i+1].score <= 1.05 &&  cblock[i+1].score > 0.0)
            {
                combined_length = 0.0;
                wscore = 0;
                lscore = 0;
                j = i+1;
                while (j < block_count && combined_length < min_show_segment_length && cblock[j].score <= 1.05 && cblock[j].score > 0.0)
                {
                    combined_length += cblock[j].length;
                    wscore += cblock[j].length * cblock[j].score;
                    lscore += cblock[j].length * cblock[j].logo;
                    j++;
                }
                wscore /= combined_length;
                lscore /= combined_length;
                if (//lscore < 0.36 &&
                    ((combined_length < min_show_segment_length / 4.0 && wscore > 0.9) ||
                     (combined_length < min_show_segment_length / 6 && wscore > 0.3) ) &&
                    cblock[j].score > 1.1 &&
                    (combined_length < min_show_segment_length / 12 ||
                     (cblock[i].f_start > after_start &&
                      cblock[j].f_end < before_end)))
                {
                    for (k = i+1; k < j; k++)
                    {
                        cblock[k].score = 99.99;
                        Debug(3, "H1 Discarding cblock %i because too short and between two weak commercial blocks.\n",
                              k);
                        cblock[k].cause |= C_H1;
                        cblock[k].more |= C_H1;
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

    if (!(disable_heuristics & (1 << (8- 1))))
    {
        for (i = 0; i < block_count-2; i++)
        {
            if ( (cblock[i].cause & (C_b | C_u ) )  &&
                    cblock[i].score > 1.05 &&
                    cblock[i].length < min_show_segment_length &&
                    (i == 0 || cblock[i-1].score <1)
               )
            {
                k = j = cblock[i].f_end;
                while (j>1 && frame[j].brightness < 16)
                    j--;
                if (k - j > 10 &&
                    F2T(k) - F2T(j) > 5.0) // If more then 5 seconds dark frames
                {

                    cblock[i].score = 0.5;
                    Debug(3, "H8 Added cblock %i because long dark sequence at end.\n", i);
                    cblock[i].cause |= C_H8;
                    cblock[i].less |= C_H8;
                }
            }
        }
    }






    if (delete_show_before_or_after_current && logo_block_count >= 80)
        Debug(10, "Too many logo blocks, disabling the delete_show_before_or_after_current processing\n");
    if (delete_show_before_or_after_current &&
            (commDetectMethod & LOGO) && connect_blocks_with_logo &&
            !reverseLogoLogic && logoPercentage > logo_fraction - 0.05 && logo_block_count < 40)
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
        if (!(disable_heuristics & (1 << (7 - 1))))
        {
            i = 0;
            while (i < block_count-1 && cblock[i].score < 1.05 &&
                    ((delete_show_before_or_after_current == 1 && cblock[i].f_end < after_start) ||
                     (delete_show_before_or_after_current > 1 && delete_show_before_or_after_current > cblock[i].length)))
            {
                j = i+1;
                cl = 0;
                combined_length = 0;
                while (cblock[j].score > 1.05 && cl + cblock[j].length < min_commercialbreak && j < block_count-1)
                {
                    combined_length += cblock[j].length;
                    cl += cblock[j].length;
                    j++;
                }
                if (cblock[j].score < 1.0 && cblock[j].length > min_show_segment_length/2 )
                {
                    cblock[i].score = 99.99;
                    Debug(3, "H7 Discarding cblock %i of %i seconds because cblock %i has also logo and small non show gap.\n",
                          i, (int)cblock[i].length, j);
                    cblock[i].cause |= C_H7;
                    cblock[i].more |= C_H7;
                    //start_deleted = true;
                    break;
                }
                i++;
            }

            i = block_count-1;
            while (i > 0 && cblock[i].score < 1.0 &&
                    ((delete_show_before_or_after_current == 1 && cblock[i].f_start > before_end) ||
                     (delete_show_before_or_after_current > 1 && delete_show_before_or_after_current > cblock[i].length)))
            {
                j = i-1;
                cl = 0;
                combined_length = 0;
                while (cblock[j].score > 1.05 && cl + cblock[j].length < min_commercialbreak && j >0)
                {
                    combined_length += cblock[j].length;
                    cl += cblock[j].length;
                    j--;
                }
                if (cblock[j].score < 1.0)
                {
                    cblock[i].score = 99.99;
                    Debug(3, "H7 Discarding cblock %i of %i seconds because cblock %i has also logo and small non show gap.\n",
                          i, (int)cblock[i].length, j);
                    cblock[i].cause |= C_H7;
                    cblock[i].more |= C_H7;
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
        if (!(disable_heuristics & (1 << (3 - 1))))
        {
            for (i = 0; i < block_count-1; i++)
            {
                if (cblock[i].score < 1.0 && cblock[i].logo < 0.1 && cblock[i].length > min_show_segment_length )
                {
                    if (cblock[i].f_end < after_start)
                    {
                        cblock[i].score *= 1.3;
                        Debug(3, "H3 Demoting cblock %i because cblock %i has no logo and others do.\n",
                              i, i);
                        cblock[i].cause |= C_H3;
                        cblock[i].more |= C_H3;

                    }
                    else if (cblock[i].f_start > before_end)
                    {
                        cblock[i].score *= 1.3;
                        Debug(3, "Demoting cblock %i because cblock %i has no logo and others do.\n",
                              i, i);
                        cblock[i].cause |= C_H3;
                        cblock[i].more |= C_H3;

                    }
                }
            }

        }
//	if (!(disable_heuristics & (1 << (3 - 1)))) {
        for (i = 1; i < block_count-1; i++)
        {
            if (logoPercentage > logo_fraction &&
                    cblock[i].score > 1.0 && cblock[i].logo < 0.1 &&
                    cblock[i].length > min_show_segment_length+4 )
            {
                if (cblock[i].f_start > after_start &&
                        cblock[i].f_end   < before_end &&
                        (cblock[i-1].score < 1.0 || cblock[i+1].score < 1.0 ))
                {
                    cblock[i].score *= 0.5;
                    Debug(3, "Promoting cblock %i because cblock %i has no logo but long and in the middle of a show.\n",
                          i, i);
                    cblock[i].cause |= C_H3;
                    cblock[i].more |= C_H3;

                }
            }
        }

//	}
    }
    if (!(disable_heuristics & (1 << (4 - 1))))
    {

        if ((commDetectMethod & LOGO) && !reverseLogoLogic && logoPercentage > logo_fraction)
        {
            i = 1;
            while (i < block_count)
            {
                if (cblock[i].score < 1 && cblock[i].b_head > 7 && CUTCAUSE(cblock[i-1].cause) == C_b)
                {
                    j = i-1;
                    k = 0;
                    while (j >= 0 && k < 5 && cblock[j].b_head > 7 && cblock[j].length < 7 && CUTCAUSE(cblock[j].cause) == C_b)
                    {
                        cblock[j].score *= 0.1;   //  Add blocks with long black periods before show
                        Debug(3, "H4 Added cblock %i because of large black gap with cblock %i\n", j, i);
                        k++;
                        cblock[j].cause |= C_H4;
                        cblock[j].less |= C_H4;
                        j--;
                    }
                }
                i++;
            }
        }
        if ((commDetectMethod & LOGO) && !reverseLogoLogic && logoPercentage > logo_fraction)
        {
            i = 0;
            while (i < block_count)
            {
                if (cblock[i].score < 1 && cblock[i].b_tail > 7 && CUTCAUSE(cblock[i].cause) == C_b)
                {
                    j = i+1;
                    k = 0;
                    while (j < block_count && k < 5 && cblock[j].b_tail > 7 && cblock[j].length < 7 && CUTCAUSE(cblock[j-1].cause) == C_b)
                    {
                        cblock[j].score *= 0.1;   //  Add blocks with long black periods before show
                        Debug(3, "H4 Added cblock %i because of large black gap with cblock %i\n", j, i);
                        k++;
                        cblock[j].cause |= C_H4;
                        cblock[j].less |= C_H4;
                        j++;
                    }
                }
                i++;
            }
        }
    }
    if (remove_silent_segments > 0 && !(disable_heuristics & (1 << (9 - 1))))
    {
        for (i = 0; i < block_count; i++)
        {
            if (cblock[i].volume<20 && cblock[i].length > remove_silent_segments )
            {
                   cblock[i].score = 5;
                    Debug(3, "H9  Demoting cblock %i because is long and has total silence\n",
                    i, i);
                    cblock[i].cause |= C_H3;
                    cblock[i].more |= C_H3;

            }
        }

    }



    if (!(disable_heuristics & (1 << (2 - 1))))
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

