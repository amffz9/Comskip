#include "legacy_detection.h"

char *CauseString(int i)
{
    static char cs[4][80];
    static int ii=0;
    char *c = &(cs[ii][0]);
    char *rc = &(cs[ii][0]);

    *c++ = (i & C_H8		? '8' : ' ');
    *c++ = (i & C_H7		? '7' : ' ');
    *c++ = (i & C_H6		? '6' : ' ');
    *c++ = (i & C_H5		? '5' : ' ');
    *c++ = (i & C_H4		? '4' : ' ');
    *c++ = (i & C_H3		? '3' : ' ');
    *c++ = (i & C_H2		? '2' : ' ');
    *c++ = (i & C_H1		? '1' : ' ');

    if (strncmp((char*)cs[ii],"       ",7))
        *c++ = '{';
    else
        *c++ = ' ';
    *c++ = (i & C_SC		? 'F' : ' ');
    *c++ = (i & C_AR		? 'A' : ' ');
    *c++ = (i & C_EXCEEDS	? 'E' : ' ');
    *c++ = (i & C_LOGO		? 'L' : (i & C_BRIGHT			? 'B': ' '));
    *c++ = (i & C_COMBINED ? 'C' : ' ');
    *c++ = (i & C_NONSTRICT? 'N' : ' ');
    *c++ = (i & C_STRICT	? 'S' : ' ');
    *c++ = (i & C_c			? 'c' : (i & C_t			? 't': ' '));
    *c++ = (i & C_l			? 'l' : (i & C_v			? 'v': ' '));
    *c++ = (i & C_s			? 's' : ' ');
    *c++ = (i & C_a			? 'a' : ' ');
    *c++ = (i & C_u			? 'u' : ' ');
    *c++ = (i & C_b			? 'b' : ' ');
    *c++ = (i & C_r			? 'r' : ' ');
    *c++ = 0;

    ii = (ii + 1) % 4;

    return(rc);
}

double ValidateBlackFrames(long reason, double ratio, int remove)
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
    const char *r = " -undefined- ";
    if (reason == C_b)
        r = "Black Frame  ";
    if (reason == C_v)
        r = "Volume       ";
    if (reason == C_s)
        r = "Scene Change ";
    if (reason == C_c)
        r = "Change       ";
    if (reason == C_u)
        r = "Uniform Frame";
    if (reason == C_a)
        r = "Aspect Ratio ";
    if (reason == C_t)
        r = "Cut Scene    ";
    if (reason == C_l)
        r = "Logo         ";

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
    while(i < black_count)
    {
        while (i < black_count && (black[i].cause & reason) == 0)
        {
            i++;
        }
        k = i;
        while (k < black_count && (black[k+1].cause & reason) != 0 && black[k+1].frame == black[k].frame+1)
        {
            k++;
        }
        if (i < black_count)
        {
            length = F2T(black[(i+k)/2].frame) - F2T(black[last].frame);
            if (length > max_commercial_size)
            {
                if (incommercial)
                {
                    incommercial = 0;
                    if (summed_length < min_commercialbreak && summed_length > 4.7 && black[(i+k)/2].frame < frame_count * 6 / 7  && black[last].frame > frame_count * 1 / 7 )
                    {
                        negative_count++;
                        Debug (10,"Negative %s cutpoint at %6i, commercial too short\n", r,black[last].frame);
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
                if (incommercial && summed_length > max_commercialbreak )
                {
                    if (black[(i+k)/2].frame < frame_count * 6 / 7)
                    {
                        negative_count++;
                        Debug (10,"Negative %s cutpoint at %6i, commercial too long\n", r,black[(i+k)/2].frame);
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
    Debug (1,"Distribution of %s cutting: %3i positive and %3i negative, ratio is %6.4f\n", r,	positive_count, negative_count, (negative_count > 0 ? (double)positive_count / (double)negative_count : 9.99));

    if ((logoPercentage < logo_fraction || logoPercentage > logo_percentile) && negative_count > 1)
    {

        Debug (1,"Confidence of %s cutting: %3i negative without good logo is too much\n", r,	negative_count);
        if (remove)
        {
            for (k = black_count - 1; k >= 0; k--)
            {
                if (black[k].cause & reason)
                {
                    black[k].cause &= ~reason;
                    if (black[k].cause == 0)
                    {
                        for (j = k; j < black_count - 1; j++)
                        {
                            black[j] = black[j + 1];
                        }
                        black_count--;
                    }
                }
            }
        }

        /*

        	if (negative_count > 1 && reason == C_v)
        	{
        		Debug(1, "Too mutch Silence Frames, disabling silence detection\n");
        		commDetectMethod &= ~SILENCE;
        	}
        	if (negative_count > 1 && reason == C_s)
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
    while(i < black_count)
    {
        total_cause = black[i].cause;
        k = i;
        while (k < black_count && black[k+1].frame == black[k].frame+1)
        {
            k++;
            total_cause |= black[k].cause;
        }
        last = (i+k)/2;
        if ((total_cause & reason) && (prev_cause & reason))
        {
            j = i-1;
            while (j > 0 && black[j-1].frame == black[j].frame - 1)
                j--;

            length = F2T(black[i].frame) - F2T(black[(i-1+j)/2].frame);
            if (length > 1.0 && length< max_commercial_size)
            {
                count++;
                if (IsStandardCommercialLength(length, F2T(i) - F2T(j)  + 0.8 , false))
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
        Debug (1,"Confidence of %s cutting: %3i out of %3i are strict, too low\n", r,	strict_count, count);
        if (remove)
        {
            for (k = black_count - 1; k >= 0; k--)
            {
                if (black[k].cause & reason)
                {
                    black[k].cause &= ~reason;
                    if (black[k].cause == 0)
                    {
                        for (j = k; j < black_count - 1; j++)
                        {
                            black[j] = black[j + 1];
                        }
                        black_count--;
                    }
                }
            }
        }
    }
    else
        Debug (1,"Confidence of %s cutting: %3i out of %3i are strict\n", r,	strict_count, count);
    return (count > 0 ? (double)strict_count / (double) count : 0);
}

//Function code blocks

bool BuildBlocks(bool recalc)
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

//	max_block_count = 80;
    max_block_count = MAX_BLOCKS;
    block_count = 0;
//	cblock = malloc(max_block_count * sizeof(block_info));

    recalculate = recalc;
    InitializeBlockArray(0);

    // If there are no black frames, nothing can be done
//	if (!black_count && !ar_block_count) return (false);

//	OutputHistogram(volumeHistogram, volumeScale, "Volume", true);

    if (!recalc)
    {
        // Eliminate frames that are too bright from black frame list
        if (intelligent_brightness)
        {
            OutputbrightHistogram();
            max_avg_brightness = black_threshold = FindBlackThreshold(black_percentile);
            Debug(1, "Setting brightness threshold to %i\n", black_threshold);
        }
        if ((intelligent_brightness && non_uniformity > 0)
//			|| 	(commDetectMethod & BLACK_FRAME && non_uniformity == 0) // Diabled
           )
        {
            OutputuniformHistogram ();
            non_uniformity = uniform_threshold = FindUniformThreshold(uniform_percentile);
            Debug(1, "Setting uniform threshold to %i\n", uniform_threshold);

            if (commDetectMethod & BLACK_FRAME)
            {
                for (i = 1; i < frame_count; i++)
                {
                    frame[i].isblack &= ~C_u;
                    if (/*!(frame[i].isblack & C_b) && */ non_uniformity > 0 && frame[i].uniform < non_uniformity && frame[i].brightness < 250 /*&& frame[i].volume < max_volume*/ )
                        InsertBlackFrame(i,frame[i].brightness,frame[i].uniform,frame[i].volume, (int)C_u);
                }
            }
        }
    }

    j = 0;
    for (i = 2; i < frame_count - 1; i++)  // frame 0 is not used
        if (frame[i-1].volume != -1 && frame[i].volume == -1 && frame[i+1].volume != -1)
            j++;
    if (j>0)
        Debug(9,"Single frames with missing audio: %d\n",j);

    if (non_uniformity < min_uniform + 100)
        non_uniformity = min_uniform + 100;

    if (framearray)  						// Find minumum volume around black frame
    {
        for (k = black_count - 1; k >= 0; k--)
        {
            if (black[k].cause == C_s || black[k].cause == C_c || black[k].cause == (C_c|C_s) )
            {
                i = black[k].frame-volume_slip;		// Find quality of silence around black frame
                if (i < 0) i = 0;
                j = black[k].frame+volume_slip;
                if (j>frame_count) j = frame_count;
                count = 0;
                for (a=i; a<j; a++)
                {
                    if (frame[a].volume < max_volume/4)
                        count += volume_slip;
                    else if (frame[a].volume < max_volume)
                        count++;

                }
                if (count > volume_slip/4)
                {
                    black[k].volume = max_volume /2;
                }
                else
                {
                    black[k].volume = max_volume *10 ;
                }
            }
            else
            {
                i = black[k].frame-volume_slip;		// Find minimum volume around black frame
                if (i < 0) i = 0;
                j = black[k].frame+volume_slip;
                if (j>frame_count) j = frame_count;
                for (a=i; a<j; a++)
                    if (frame[a].volume >= 0)
                        if (black[k].volume > frame[a].volume)
                            black[k].volume = frame[a].volume;
            }
        }
        for (k = ar_block_count - 1; k >= 0; k--)
        {
            i = ar_block[k].end-volume_slip;
            if (i < 0) i = 0;
            j = ar_block[k].end+volume_slip;
            if (j>frame_count) j = frame_count;
            ar_block[k].volume = frame[i].volume;
            for (a=i; a<j; a++)
                if (frame[a].volume >= 0)
                    if (ar_block[k].volume > frame[a].volume)
                    {
                        ar_block[k].volume = frame[a].volume;
                        ar_block[k].end = a;
                        if (k < ar_block_count - 1)
                            ar_block[k+1].start = a;
                    }
        }

    }
    for (k = 1; k < 255; k++)
    {
        if (volumeHistogram[k] > 10)
        {
            min_volume = (k-1)*volumeScale;
            break;
        }
    }

    for (k = 1; k < 255; k++)
    {
        if (uniformHistogram[k] > 10)
        {
            min_uniform = (k-1)*UNIFORMSCALE;
            break;
        }
    }

    for (k = 0; k < 255; k++)
    {
        if (brightHistogram[k] > 1)
        {
            min_brightness_found = k;
            break;
        }
    }
    if (max_volume > 0)
    {
        for (k = black_count - 1; k >= 0; k--)
        {
            if ((black[k].cause & C_t) != 0)
                continue;
            if (black[k].volume >  max_volume
//				&&
//				( black[k].frame > 10 && (int)frame[black[k].frame-2].brightness < (int)frame[black[k].frame].brightness + 50 &&
//				 black[k].frame < frame_count - 10 && (int)frame[black[k].frame+2].brightness < (int) frame[black[k].frame].brightness + 50 )
//			   || black[k].volume >  max_volume * 1.5
               )
            {

                Debug
                (
                    12,
                    "%i - Removing black frame %i, from black frame list because volume %i is more than %i, brightness %i, uniform %i\n",
                    k,
                    black[k].frame,
                    black[k].volume,
                    max_volume,
                    black[k].brightness,
                    black[k].uniform
                );

                for (j = k; j < black_count - 1; j++)
                {
                    black[j] = black[j + 1];
                }

                black_count--;
            }
        }
    }

    for (k = black_count - 1; k >= 0; k--)
    {
        if ((black[k].cause & C_t) != 0)
            continue;

        if ((black[k].cause & C_r) != 0)
            continue;

        if ((black[k].cause & C_b) && black[k].brightness > max_avg_brightness)
        {

            			Debug
            			(
            			12,
            			"%i - Removing black frame %i, from black frame list because %i is more than %i, uniform %i\n",
            			k,
            			black[k].frame,
            			black[k].brightness,
            			max_avg_brightness,
            			black[k].uniform
            			);

            for (j = k; j < black_count - 1; j++)
            {
                black[j] = black[j + 1];
            }

            black_count--;
        }
    }
    if (non_uniformity > 0)
    {
        for (k = black_count - 1; k >= 0; k--)
        {
            if ((black[k].cause & C_t) != 0)
                continue;
            if ((black[k].cause & C_u) && black[k].uniform > non_uniformity)
            {
                Debug
                (
                12,
                "%i - Removing uniform frame %i, from black frame list because %i is more than %i, brightness %i\n",
                k,
                black[k].frame,
                black[k].uniform,
                non_uniformity,
                black[k].brightness
                );

                for (j = k; j < black_count - 1; j++)
                {
                    black[j] = black[j + 1];
                }

                black_count--;
            }
        }
    }

    if (cut_on_ar_change==2)
    {
        if (logoPercentage > logo_fraction && logoPercentage < logo_percentile)
            cut_on_ar_change = 1;
//		else
//			ar_wrong_modifier=1;
    }

    if (cut_on_ar_change==1)
    {
//		if (logoPercentage < logo_fraction || logoPercentage > logo_percentile)
//			cut_on_ar_change = 2;
    }

    if (((commDetectMethod & LOGO) && cut_on_ar_change ) || cut_on_ar_change >= 2)
    {
//	if (cut_on_ar_change ) {
        for (i = 0; i < ar_block_count; i++)
        {
            if ((cut_on_ar_change == 1 || ar_block[i].volume < max_volume) &&
                    ar_block[i].ar_ratio != AR_UNDEF && ar_block[i+1].ar_ratio != AR_UNDEF)
            {
                a = ar_block[i].end;
//					if (a > 20 * fps)
                InsertBlackFrame(a,frame[a].brightness,frame[a].uniform,frame[a].volume, C_a);
            }
        }
    }

    if ( cut_on_ac_change )
    {
        for (i = 0; i < ac_block_count; i++)
        {
            a = ac_block[i].end;
            InsertBlackFrame(a,frame[a].brightness,frame[a].uniform,frame[a].volume, C_r);
        }
    }


    if (ValidateBlackFrames(C_b, 3.0, false) < 1 / 3.0)
        Debug(8, "Black Frame cutting too low\n");

    if (validate_scenechange /* || (logoPercentage < logo_fraction || logoPercentage > logo_percentile) */)
        ValidateBlackFrames(C_s, ((logoPercentage < logo_fraction || logoPercentage > logo_percentile) ? 1.2 : 3.5), true);

    //	ValidateBlackFrames(C_c, 3.0, true);

    if (validate_uniform)
        ValidateBlackFrames(C_u, ((logoPercentage < logo_fraction || logoPercentage > logo_percentile) ? 1.2 : 3.0), true);


    if (commDetectMethod & SILENCE)
    {
        k = 0;
        for (i = 0; i < frame_count; i++)
        {
            if (frame[i].volume < max_volume) k++;
        }
        /*
        		if (k * 100 / frame_count > 25) {
        			Debug(8, "Too mutch Silence Frames (%d%%), disabling silence detection\n", k * 100 / frame_count);

        			ValidateBlackFrames(C_v, 1.0, true);
        			commDetectMethod &= ~SILENCE;
        			validate_silence = 0;
        		} else
        */		if (validate_silence)
            ValidateBlackFrames(C_v, 3.0, true);
    }

//		if (logoPercentage < logo_fraction)
//	if (cut_on_ar_change == 2)
//		ValidateBlackFrames(C_a, 3.0, true);


    Debug(8, "Black Frame List\n---------------------------\nBlack Frame Count = %i\nnr \tframe\tpts\tbright\tuniform\tvolume\t\tcause\tdimcount  bright   type\n", black_count);
    for (k = 0; k < black_count; k++)
    {
        Debug(8, "%3i\t%6i\t%8.3f\t%6i\t%6i\t%6i\t%6s\t%6i\t%6i\t%c\n", k, black[k].frame, get_frame_pts(black[k].frame), black[k].brightness, black[k].uniform, black[k].volume,&(CauseString(black[k].cause)[10]), frame[black[k].frame].dimCount, frame[black[k].frame].hasBright, frame[black[k].frame].pict_type);
        if (k+1 < black_count && black[k].frame+1 != black[k+1].frame)
            Debug(8, "-----------------------------\n");

    }


    // add black frame at end to enable usage of last cblock
    InsertBlackFrame(framesprocessed,0,0,0,C_b);
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
    a = ar_block_count;			// Don't cut on AR when logo disabled
    cause = 0;
    block_count = 0;
    prev_start = 1;
    prev_head = 0;

    while(i < black_count || a < ar_block_count)
    {
        if (!(commDetectMethod & LOGO) && i < black_count && (black[i].cause & (C_s | C_l)))
        {
//			i++; // Skip logo cuts and brighness cuts when not enough logo detected
//			goto again;
        }
        cause = 0;
        b_start = black[i].frame;
        cause |= black[i].cause;
        b_end = b_start;
        j = i + 1;

        v_count = 0;
        b_count = 0;
        black_start = 0;
        black_end = 0;
        //Find end of next black cblock
        while(j < black_count && (F2T(black[j].frame) - F2T(b_end) < 1.0 ))   //Allow for 2 missing black frames
        {
            if (black[j].frame - b_end > 2 &&
                    (((black[j].cause & (C_v)) != 0 &&  (cause & (C_v)) == 0) ||
                     ((black[j].cause & (C_v)) == 0 &&  (cause & (C_v)) != 0)))
            {

                Debug
                (
                    6,
                    "At frame %i there is a gap of %i frames in the blackframe list\n",
                    black[j].frame,
                    black[j].frame - b_end
                );
            }

            if ((black[j].cause & (C_b | C_s | C_u | C_r)) != 0)
            {
                b_count++;
                if (black_start == 0)
                    black_start = black[j].frame;
                black_end = black[j].frame;

            }
            if ((black[j].cause & (C_v)) != 0)
                v_count++;
            if (black[j].cause == C_a)
            {
                cause |= black[j].cause;
                j++;
            }
            else if (cause == C_a)
            {
                cause |= black[j].cause;
                b_start = b_end = black[j++].frame;
            }
            else
            {
                cause |= black[j].cause;
                b_end = black[j++].frame;
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
        cblock[block_count].cause = cause;

        //Do it this way for in roundoff problems
        b_counted = (b_end - b_start + 1)/2;

        cblock[block_count].b_head = prev_head;
        cblock[block_count].f_start = prev_start - cblock[block_count].b_head;
        if (b_end == framesprocessed)
            cblock[block_count].f_end = framesprocessed;
        else
            cblock[block_count].f_end = b_start + b_counted - 1;
        cblock[block_count].b_tail = b_counted;		//half on the tail of this cblock
        cblock[block_count].bframe_count = cblock[block_count].b_head + cblock[block_count].b_tail;
        cblock[block_count].length = F2T(cblock[block_count].f_end) - F2T(cblock[block_count].f_start);

        //If first cblock is < 1 sec. throw it away
        if( block_count > 0 ||
                F2L( cblock[block_count].f_end, cblock[block_count].f_start) > 1.0 ||
                cblock[block_count].f_end == framesprocessed
          )
        {

            			Debug(12, "Creating cblock %i From %i (%i) to %i (%i) because of %s with %i head and %i tail\n",
            					block_count, cblock[block_count].f_start, (cblock[block_count].f_start + cblock[block_count].b_head),
            					cblock[block_count].f_end, (cblock[block_count].f_end - cblock[block_count].b_tail),
            					CauseString(cause),
            					cblock[block_count].b_head, cblock[block_count].b_tail);

            block_count++;
            InitializeBlockArray(block_count);
            prev_start = b_end + 1;							//cblock starts at end of black initially
            prev_head = b_end - b_start - b_counted + 1;	//remaining black from previous cblock tail
        }
    }


#if 1
    //Combine blocks with less than minimum black between them
    for (i = block_count-1; i >= 1; i--)
    {

        unsigned int bfcount = cblock[i].b_head + cblock[i-1].b_tail;

        if (bfcount < min_black_frames_for_break && cblock[i-1].cause == C_b)
        {

            Debug(10, "Combining blocks %i and %i at %i because there are only %i black frames separating them.\n",
                  i-1, i, cblock[i-1].f_end , bfcount);

            cblock[i-1].f_end	= cblock[i].f_end;
            cblock[i-1].b_tail	= cblock[i].b_tail;
            cblock[i-1].length	= F2L(cblock[i-1].f_end, cblock[i-1].f_start);
            cblock[i-1].cause	= cblock[i].cause;

            for (k = i; k < block_count-1; k++)
            {
                cblock[k]				= cblock[k+1];
            }
            block_count--;
        }
    }
#endif
    return (true);
}


void FindLogoThreshold()
{
    int i;
    int buckets = 20;
    int counter = 0;
    if (framearray)
    {
        for (i = 1; i < frame_count; i += 1 /*(int) fps */ )
        {
            logoHistogram[(int)(frame[i].currentGoodEdge * (buckets - 1))]++;
        }

        OutputLogoHistogram(buckets);
        counter = 0;
        for (i = 0; i < buckets; i++)
        {
            counter += logoHistogram[i];
            if (100 * counter / frame_count > 40)
                break;
        }
        if (i < buckets/2)
            i = buckets * 3 / 4;
        else
        {
            if (logoHistogram[i - 2] < logoHistogram[i])
                i -= 2;
            if (logoHistogram[i - 1] < logoHistogram[i])
                i -= 1;
            if (logoHistogram[i - 1] < logoHistogram[i])
                i -= 1;
            if (logoHistogram[i - 1] < logoHistogram[i])
                i -= 1;
        }
        logo_quality = ((double) i + 0.5) / (double) buckets;
        Debug(8, "Set Logo Quality = %.5f\n", logo_quality);

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
    if (logo_threshold == 0)
    {
        logo_threshold = logo_quality;
    }
}

void CleanLogoBlocks()
{
    int i,k,n;
//	double stdev;
    int sum_brightness,v,b, sum_volume,s,sum_silence,sum_uniform;
    double sum_brightness2;
    int sum_delta;
#if 1
    if ((commDetectMethod & LOGO /* || startOverAfterLogoInfoAvail==0 */ ) &&! reverseLogoLogic && connect_blocks_with_logo)
    {
        //Combine blocks with both logo
        for (i = block_count-1; i >= 1; i--)
        {
            if (CheckFrameForLogo(cblock[i-1].f_end) &&
                    CheckFrameForLogo(cblock[i].f_start) )
            {

                Debug(6, "Joining blocks %i and %i at frame %i because they both have a logo.\n",
                      i-1, i, cblock[i-1].f_end);

                cblock[i-1].f_end	= cblock[i].f_end;
                cblock[i-1].b_tail	= cblock[i].b_tail;
                if (cblock[i].length > cblock[i-1].length)
                    cblock[i-1].ar_ratio = cblock[i].ar_ratio;	// Use AR of longest cblock
                cblock[i-1].length	= F2L(cblock[i-1].f_end, cblock[i-1].f_start);
                cblock[i-1].cause	= cblock[i].cause;

                for (k = i; k < block_count-1; k++)
                {
                    cblock[k] = cblock[k+1];
                }
                block_count--;
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
    avg_brightness = 0;
    avg_volume = 0;
    maxi_volume = 0;
    avg_silence = 0;
    avg_uniform = 0;
    avg_schange = 0.0;
    for (i = 0; i < block_count; i++)
    {
        sum_brightness = 0;
        sum_volume = 0;
        sum_silence = 0;
        sum_uniform = 0;
        sum_brightness2 = 0.0;
        sum_delta = 0;
        if (framearray)
        {
            for (k = cblock[i].f_start+1; k < cblock[i].f_end; k++)
            {

//				b = frame[k].brightness;
                b = abs(frame[k].brightness - frame[k-1].brightness);
                v = frame[k].volume;
                if (maxi_volume < v)
                    maxi_volume = v;
                s = (frame[k].volume < max_volume ? 0 : 99);
                sum_brightness += b;
                sum_volume += v;
                sum_silence += s;
                sum_uniform += abs(frame[k].uniform - frame[k-1].uniform);
                sum_brightness2 += b*b;
                sum_delta += abs(frame[k].brightness - frame[k-1].brightness);
            }
        }
        n = cblock[i].f_end - cblock[i].f_start+1;
        if (n>0) {
        cblock[i].brightness = sum_brightness * 1000 / n;
        cblock[i].volume = sum_volume / n;
        cblock[i].silence = sum_silence / n;
        cblock[i].uniform = sum_uniform / n;
        }
        if ((cblock[i].schange_count = CountSceneChanges(cblock[i].f_start, cblock[i].f_end)))
        {
            cblock[i].schange_rate = (double)cblock[i].schange_count / n;
        }
        else
            cblock[i].schange_rate = 0.0;

        cblock[i].stdev =
//			sqrt( (n*sum_brightness2 - sum_brightness*sum_brightness)/ (n * (n-1)));
            100* sum_delta / n;
        avg_brightness += sum_brightness * 1000;
        avg_volume += sum_volume;
        avg_silence += sum_silence;
        avg_uniform += sum_uniform;
        avg_schange += cblock[i].schange_rate*n;
    }
    n = cblock[block_count-1].f_end - cblock[0].f_start;
    if (n>0) {
        avg_brightness /= n;
        avg_volume /= n;
        avg_silence /= n;
        avg_uniform /= n;
        avg_schange /= n;
    }
//	Debug(1, "Average brightness is %i\n",avg_brightness);
//	Debug(1, "Average volume is %i\n",avg_volume);

}

#define LOGO_BORDER 5
void InitScanLines()
{
    int i;
    for (i = 0; i < height; i++)
    {
        if (i < clogoMinY - LOGO_BORDER || i > clogoMaxY + LOGO_BORDER)
        {
            lineStart[i] = border;
            lineEnd[i] = videowidth-1-border;
        }
        else
        {
            if ( clogoMinX > videowidth - clogoMaxX)   // Most pixels left of the logo
            {
                lineStart[i] = border;
                lineEnd[i] = MAX(0,clogoMinX-LOGO_BORDER);
            }
            else
            {
                lineStart[i] = MIN(videowidth-1,clogoMaxX+LOGO_BORDER);
                lineEnd[i] = videowidth-1-border;
            }
        }
    }
    for (i = height; i < MAXHEIGHT; i++)
    {
        lineStart[i] = 0;
        lineEnd[i] = 0;
    }
}

void InitHasLogo()
{

    int x,y;
    memset(haslogo, 0, MAXWIDTH*MAXHEIGHT*sizeof(char));
    for (y = MAX(0,clogoMinY - LOGO_BORDER); y < MIN(MAXHEIGHT,clogoMaxY + LOGO_BORDER); y++)
    {
        for (x = MAX(0,clogoMinX-LOGO_BORDER); x < MIN(MAXWIDTH,clogoMaxX + LOGO_BORDER) ; x++)
        {
            haslogo[y*width+x] = 1;
        }
    }
}



