#include "legacy_detection.h"

void PrintLogoFrameGroups(void)
{
    int		i,l;
    double  cl;
    int		f,t;
    int		count = 0;

    Debug(2, "\nLogos detected on the following frames\n--------------------------------------\n");
    count = 0;
    for (i = 0; i < logo_block_count; i++)
    {
        f = FindBlock(logo_block[i].start);
        t = FindBlock(logo_block[i].end-2);
        if (f<0) f = 0;
        if (t<0) t = 0;
        if (t < 0)
        {
            Debug (2, "Panic\n");
            break;
        }
        if (f < 0)
        {
            Debug (2, "Panic\n");
            break;
        }
        Debug(
            2,
            "Logo start - %6i\tend - %6i\tlength - %s\tbefore:%.1f s\t after:%.1f s\n",
            logo_block[i].start,
            logo_block[i].end,
            dblSecondsToStrMinutes(F2L(logo_block[i].end, logo_block[i].start)),
            F2L(logo_block[i].start, cblock[f].f_start),
            F2L(cblock[t].f_end, logo_block[i].end)
        );

        count += logo_block[i].end - logo_block[i].start + 1;

    }
    for (i = 0; i < logo_block_count-1; i++)
    {
        f = logo_block[i].end;
        t = logo_block[i+1].start;
        if (max_logo_gap < F2L(t,f))
            max_logo_gap = F2L(t,f);
        f = FindBlock(logo_block[i].end);
        t = FindBlock(logo_block[i+1].start);
        for (l = f+1; l < t; l++)
        {
            if (max_nonlogo_block_length < cblock[l].length)
                max_nonlogo_block_length = cblock[l].length;
        }
    }
    for (i = 0; i < logo_block_count-1; i++)
    {
        f = FindBlock(logo_block[i].start);
        t = FindBlock(logo_block[i].end);
        if (F2L(logo_block[i].end, logo_block[i].start) > max_nonlogo_block_length )
        {
            cl = F2L(cblock[f].f_end, logo_block[i].start);
            if (cl < cblock[f].length / 10 )
            {
                if (cl > logo_overshoot )
                    logo_overshoot = cl;
            }
            cl = F2L(logo_block[i].end, cblock[t].f_start);
            if (cl < cblock[t].length / 10 )
            {
                if (cl > logo_overshoot)
                    logo_overshoot = cl;
            }
        }
    }
    if (logo_overshoot > 0)
        logo_overshoot = logo_overshoot + 1 + shrink_logo;
    else
        logo_overshoot = shrink_logo;
}

void PrintCCBlocks(void)
{
    int i, j;
    Debug(2, "Combining CC Blocks...\n");
    for (i = cc_block_count - 1; i > 0; i--)
    {
        if (F2L(cc_block[i].end_frame, cc_block[i].start_frame) < 1.0)
        {
            Debug(
                4,
                "Removing cc cblock %i because the length is %.2f.\n",
                i,
                F2L(cc_block[i].end_frame, cc_block[i].start_frame)
            );
            for (j = i; j < cc_block_count - 1; j++)
            {
                cc_block[j].start_frame = cc_block[j + 1].start_frame;
                cc_block[j].end_frame = cc_block[j + 1].end_frame;
                cc_block[j].type = cc_block[j + 1].type;
            }

            cc_block_count--;
        }
    }

    Debug(2, "CC's detected on the following frames - %i total blocks\n--------------------------------------\n", cc_block_count);
    Debug(
        2,
        " 0 - CC start - %6i\tend - %6i\ttype - %s",
        cc_block[0].start_frame,
        cc_block[0].end_frame,
        CCTypeToStr(cc_block[0].type)
    );
    Debug(2, "\tlength - %s\n", dblSecondsToStrMinutes(F2L(cc_block[0].end_frame, cc_block[0].start_frame)));
    cc_count[cc_block[0].type] += cc_block[0].end_frame - cc_block[0].start_frame + 1;

    for (i = 1; i < cc_block_count; i++)
    {
        Debug(
            2,
            "%2i - CC start - %6i\tend - %6i\ttype - %s",
            i,
            cc_block[i].start_frame,
            cc_block[i].end_frame,
            CCTypeToStr(cc_block[i].type)
        );
        Debug(2, "\tlength - %s\n", dblSecondsToStrMinutes(F2L(cc_block[i].end_frame, cc_block[i].start_frame)));
        cc_count[cc_block[i].type] += cc_block[i].end_frame - cc_block[i].start_frame + 1;
    }

    Debug(2, "\nCaption sums\n---------------------------\n");
    Debug(
        2,
        "Pop on captions:   %6i:%5.2f - %s\n",
        cc_count[POPON],
        ((double)cc_count[POPON] / (double)framesprocessed) * 100.0,
        dblSecondsToStrMinutes(cc_count[POPON] / fps)
    );
    Debug(
        2,
        "Roll up captions:  %6i:%5.2f - %s\n",
        cc_count[ROLLUP],
        ((double)cc_count[ROLLUP] / (double)framesprocessed) * 100.0,
        dblSecondsToStrMinutes(cc_count[ROLLUP] / fps)
    );
    Debug(
        2,
        "Paint on captions: %6i:%5.2f - %s\n",
        cc_count[PAINTON],
        ((double)cc_count[PAINTON] / (double)framesprocessed) * 100.0,
        dblSecondsToStrMinutes(cc_count[PAINTON] / fps)
    );
    Debug(
        2,
        "No captions:       %6i:%5.2f - %s\n",
        cc_count[NONE],
        ((double)cc_count[NONE] / (double)framesprocessed) * 100.0,
        dblSecondsToStrMinutes(cc_count[NONE] / fps)
    );
    for (i = 0; i <= 4; i++)
    {
        if (cc_count[i] > cc_count[most_cc_type])
        {
            most_cc_type = i;
        }
    }

    Debug(2, "The %s type of closed captions were determined to be the most common.\n", CCTypeToStr(most_cc_type));
}

/*
static edge_inc = 1;
static edge_dec = 20;


void EdgeCount(unsigned char* frame_ptr) {
	int				i,index;
	int				x;
	int				y;
	unsigned char	herePixel;
	static int framecnt;

	edge_count = 0;
	if (aggressive_logo_rejection) {
		for (y = edge_radius + (int)(height * borderIgnore); y < (subtitles? height/2 : (height - edge_radius - (int)(height * borderIgnore))); y++) {
			for (x = edge_radius + (int)(width * borderIgnore); x < (width - edge_radius - (int)(width * borderIgnore)); x++) {
				herePixel = frame_ptr[y * width + x];
				if (
					(abs(frame_ptr[y * width + (x - edge_radius)] - herePixel) >= edge_level_threshold)
					) {
					if (hor_edgecount[y * width + x] <= num_logo_buffers)
						hor_edgecount[y * width + x]++;
					else
						edge_count++;
				} else
					hor_edgecount[y * width + x] = 0;

				if (
					(abs(frame_ptr[(y - edge_radius) * width + x] - herePixel) >= edge_level_threshold)
					) {
					if (ver_edgecount[y * width + x] <= num_logo_buffers)
						ver_edgecount[y * width + x]++;
					else
						edge_count++;
				} else
					ver_edgecount[y * width + x] = 0;
			}
		}
	} else {
		for (y = edge_radius + (int)(height * borderIgnore); y < (subtitles? height/2 : (height - edge_radius - (int)(height * borderIgnore))); y++) {
			for (x = edge_radius + (int)(width * borderIgnore); x < (width - edge_radius - (int)(width * borderIgnore)); x++) {
				herePixel = frame_ptr[y * width + x];
				if (
					(abs(frame_ptr[y * width + (x - edge_radius)] - herePixel) >= edge_level_threshold) ||
					(abs(frame_ptr[y * width + (x + edge_radius)] - herePixel) >= edge_level_threshold)
					) {
					if (hor_edgecount[y * width + x] < num_logo_buffers)
						hor_edgecount[y * width + x]++;
					else
						edge_count++;
				} else
					hor_edgecount[y * width + x] = 0;

				if (
					(abs(frame_ptr[(y - edge_radius) * width + x] - herePixel) >= edge_level_threshold) ||
					(abs(frame_ptr[(y + edge_radius) * width + x] - herePixel) >= edge_level_threshold)
					) {
					if (ver_edgecount[y * width + x] < num_logo_buffers)
						ver_edgecount[y * width + x]++;
					else
						edge_count++;
				} else
					ver_edgecount[y * width + x] = 0;
			}
		}
	}
	if (edge_count > 350)
		logoBuffersFull = true;
}

*/

#define TEST_HEDGE1(FRAME,X,Y)	(abs(FRAME[(Y) * width + (X) - edge_radius]   - FRAME[(Y) * width + (X) + edge_radius]  ) >= edge_level_threshold)
#define TEST_VEDGE1(FRAME,X,Y)	(abs(FRAME[((Y) - edge_radius) * width + (X)] - FRAME[((Y) + edge_radius) * width + (X)]) >= edge_level_threshold)

#define TEST_HEDGE0(FRAME,X,Y)  (abs(FRAME[(Y) * width + (X) - edge_radius]   - FRAME[(Y) * width + (X)]  ) >= edge_level_threshold) || \
								(abs(FRAME[(Y) * width + (X) + edge_radius]   - FRAME[(Y) * width + (X)]  ) >= edge_level_threshold)

#define TEST_VEDGE0(FRAME,X,Y)	(abs(FRAME[((Y) - edge_radius) * width + (X)] - FRAME[((Y)) * width + (X)]) >= edge_level_threshold) || \
								(abs(FRAME[((Y) + edge_radius) * width + (X)] - FRAME[((Y)) * width + (X)]) >= edge_level_threshold)

#define TEST_HEDGE2(FRAME,X,Y)  (abs((FRAME[(Y) * width + (X) - edge_radius - 1] + FRAME[(Y) * width + (X) - edge_radius] + FRAME[(Y) * width + (X) - edge_radius + 1]) - \
									 (FRAME[(Y) * width + (X) + edge_radius - 1] + FRAME[(Y) * width + (X) + edge_radius] + FRAME[(Y) * width + (X) + edge_radius + 1])   )/3 >= edge_level_threshold)

#define TEST_VEDGE2(FRAME,X,Y)	(abs((FRAME[((Y) - edge_radius - 1) * width + (X)] + FRAME[((Y) - edge_radius) * width + (X)] + FRAME[((Y) - edge_radius + 1) * width + (X)]) - \
									 (FRAME[((Y) + edge_radius - 1) * width + (X)] + FRAME[((Y) + edge_radius) * width + (X)] + FRAME[((Y) + edge_radius + 1) * width + (X)])   )/3 >= edge_level_threshold)


#define TEST_HEDGE3(FRAME,X,Y)	(abs((\
FRAME[((Y)-edge_radius)*width+(X)-edge_radius]-FRAME[((Y)-edge_radius)*width+(X)+edge_radius] +\
FRAME[((Y)            )*width+(X)-edge_radius]-FRAME[((Y)            )*width+(X)+edge_radius] +\
FRAME[((Y)+edge_radius)*width+(X)-edge_radius]-FRAME[((Y)+edge_radius)*width+(X)+edge_radius])\
) >= edge_level_threshold)

#define TEST_VEDGE3(FRAME,X,Y)	(abs((\
FRAME[((Y)-edge_radius)*width+(X)-edge_radius]-FRAME[((Y)+edge_radius)*width+(X)-edge_radius] +\
FRAME[((Y)-edge_radius)*width+(X)            ]-FRAME[((Y)+edge_radius)*width+(X)            ] +\
FRAME[((Y)-edge_radius)*width+(X)+edge_radius]-FRAME[((Y)+edge_radius)*width+(X)+edge_radius])\
) >= edge_level_threshold)


#define AR_DIST	20


void EdgeDetect(unsigned char* frame_ptr, int maskNumber)
{
    int				x;
    int				y;
    //	unsigned char	temp[MAXWIDTH * MAXHEIGHT];
//	memset(for (i = 0; i <= (width * height); i++) temp[i] = 0;
    hedge_count = 0;
    vedge_count = 0;
#ifdef MAXMIN_LOGO_SEARCH
    if (maskNumber == 0)
    {
        memset(max_br, 0, sizeof(max_br));
        memset(min_br, 255, sizeof(max_br));
    }
    for (y = (logo_at_bottom ? height/2 : edge_radius + (int)(height * borderIgnore)); y < (subtitles? height/2 : (height - edge_radius - (int)(height * borderIgnore))); y++)
    {
        for (x = max(edge_radius + (int)(width * borderIgnore), minX+AR_DIST); x < min((width - edge_radius - (int)(width * borderIgnore)),maxX-AR_DIST); x++)
        {
            herePixel = frame_ptr[y * width + x];
            if (herePixel < min_br[y * width + x])
                min_br[y * width + x] = herePixel;
            if (herePixel > max_br[y * width + x])
                max_br[y * width + x] = herePixel;
        }
    }
#endif
#if MULTI_EDGE_BUFFER
    memset(horiz_edges[maskNumber], 0, width * height);
    memset(vert_edges[maskNumber], 0, width * height);
    for (y = (logo_at_bottom ? height/2 : edge_radius + (int)(height * borderIgnore)); y < (subtitles? height/2 : (height - edge_radius - (int)(height * borderIgnore))); y++)
    {
        for (x = max(edge_radius + (int)(width * borderIgnore), minX+AR_DIST); x < min((width - edge_radius - (int)(width * borderIgnore)),maxX-AR_DIST); x++)
        {
            herePixel = frame_ptr[y * width + x];
            if ((abs(frame_ptr[y * width + (x - edge_radius)] - herePixel) >= edge_level_threshold) ||
                    (abs(frame_ptr[y * width + (x + edge_radius)] - herePixel) >= edge_level_threshold))
            {
                horiz_edges[maskNumber][y * width + x] = 1;
            }

            if ((abs(frame_ptr[(y - edge_radius) * width + x] - herePixel) >= edge_level_threshold) ||
                    (abs(frame_ptr[(y + edge_radius) * width + x] - herePixel) >= edge_level_threshold))
            {
                vert_edges[maskNumber][y * width + x] = 1;
            }
        }
    }
#else
    if (aggressive_logo_rejection==1)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if (TEST_HEDGE1(frame_ptr,x,y))
                {
                    if (hor_edgecount[y * width + x] < num_logo_buffers)
                        hor_edgecount[y * width + x]++;
                    else
                        edge_count++;
                }
                else
                    hor_edgecount[y * width + x] = 0;
                if (TEST_VEDGE1(frame_ptr,x,y))
                {
                    if (ver_edgecount[y * width + x] < num_logo_buffers)
                        ver_edgecount[y * width + x]++;
                    else
                        edge_count++;
                }
                else
                    ver_edgecount[y * width + x] = 0;
            }
        }
    }
    else if (aggressive_logo_rejection==2)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if (TEST_HEDGE2(frame_ptr,x,y))
                {
                    if (hor_edgecount[y * width + x] < num_logo_buffers)
                        hor_edgecount[y * width + x]++;
                    else
                        edge_count++;
                }
                else
                    hor_edgecount[y * width + x] = 0;
                if (TEST_VEDGE2(frame_ptr,x,y))
                {
                    if (ver_edgecount[y * width + x] < num_logo_buffers)
                        ver_edgecount[y * width + x]++;
                    else
                        edge_count++;
                }
                else
                    ver_edgecount[y * width + x] = 0;
            }
        }
//	printf("%6d %6d\n", hedge_count, vedge_count);
    }
    else if (aggressive_logo_rejection==3)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if (TEST_HEDGE3(frame_ptr,x,y))
                {
                    if (hor_edgecount[y * width + x] < num_logo_buffers)
                        hor_edgecount[y * width + x]++;
                    else
                        edge_count++;
                }
                else
                    hor_edgecount[y * width + x] = 0;
                if (TEST_VEDGE3(frame_ptr,x,y))
                {
                    if (ver_edgecount[y * width + x] < num_logo_buffers)
                        ver_edgecount[y * width + x]++;
                    else
                        edge_count++;
                }
                else
                    ver_edgecount[y * width + x] = 0;
            }
        }
    }
    else if (aggressive_logo_rejection==4)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if ((/*frame_ptr[y * width + x - edge_radius] > 50 && */ frame_ptr[y * width + x - edge_radius] < 200) || ( /*frame_ptr[y * width + x + edge_radius] > 50 && */ frame_ptr[y * width + x + edge_radius] < 200) )
                {
                    if (TEST_HEDGE0(frame_ptr,x,y))
                    {
                        if (hor_edgecount[y * width + x] < num_logo_buffers)
                            hor_edgecount[y * width + x]++;
                        else
                            edge_count++;
                    }
                    else if (frame_ptr[y * width + x] < 200)
                        hor_edgecount[y * width + x] = 0;
                }
                if ((/*frame_ptr[(y- edge_radius) * width + x ] > 50 && */ frame_ptr[(y- edge_radius) * width + x ] < 200) || ( /*frame_ptr[(y+ edge_radius) * width + x ] > 50 && */ frame_ptr[(y+ edge_radius) * width + x ] < 200) )
                {
                    if (TEST_VEDGE0(frame_ptr,x,y))
                    {
                        if (ver_edgecount[y * width + x] < num_logo_buffers)
                            ver_edgecount[y * width + x]++;
                        else
                            edge_count++;
                    }
                    else if (frame_ptr[y * width + x] < 200)
                        ver_edgecount[y * width + x] = 0;
                }
            }
        }
    }
    else
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if ((/*frame_ptr[y * width + x - edge_radius] > 50 && */ frame_ptr[y * width + x - edge_radius] < 200) || ( /*frame_ptr[y * width + x + edge_radius] > 50 && */ frame_ptr[y * width + x + edge_radius] < 200) )
                {
                    if (TEST_HEDGE0(frame_ptr,x,y))
                    {
                        if (hor_edgecount[y * width + x] < num_logo_buffers)
                            hor_edgecount[y * width + x]++;
                        else
                            edge_count++;
                    }
                    else
                        hor_edgecount[y * width + x] = 0;
                }
                if ((/*frame_ptr[(y- edge_radius) * width + x ] > 50 && */ frame_ptr[(y- edge_radius) * width + x ] < 200) || ( /*frame_ptr[(y+ edge_radius) * width + x ] > 50 && */ frame_ptr[(y+ edge_radius) * width + x ] < 200) )
                {
                    if (TEST_VEDGE0(frame_ptr,x,y))
                    {
                        if (ver_edgecount[y * width + x] < num_logo_buffers)
                            ver_edgecount[y * width + x]++;
                        else
                            edge_count++;
                    }
                    else
                        ver_edgecount[y * width + x] = 0;
                }
            }
        }
    }
#endif
}



double CheckStationLogoEdge(unsigned char* testFrame)
{
    int		index;
    int		x;
    int		y;
    int		testEdges = 0;

    int goodEdges = 0;

    currentGoodEdge = 0.0;
    if (videowidth < clogoMinX || height < clogoMinY)
    {
        // No logo possible as frame size if different from where logo was found
    }
    else if (aggressive_logo_rejection == 1)
    {
        for (y = clogoMinY; y <= clogoMaxY; y += edge_step)
        {
            for (x = clogoMinX; x <= clogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (choriz_edgemask[index])
                {
                    if (TEST_HEDGE1(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (cvert_edgemask[index])
                {
                    if (TEST_VEDGE1(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (aggressive_logo_rejection == 2)
    {
        for (y = clogoMinY; y <= clogoMaxY; y += edge_step)
        {
            for (x = clogoMinX; x <= clogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (choriz_edgemask[index])
                {
                    if (TEST_HEDGE2(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (cvert_edgemask[index])
                {
                    if (TEST_VEDGE2(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (aggressive_logo_rejection == 3)
    {
        for (y = clogoMinY; y <= clogoMaxY; y += edge_step)
        {
            for (x = clogoMinX; x <= clogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (choriz_edgemask[index])
                {
                    if (TEST_HEDGE3(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (cvert_edgemask[index])
                {
                    if (TEST_VEDGE3(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (aggressive_logo_rejection == 4)
    {
        for (y = clogoMinY; y <= clogoMaxY; y += edge_step)
        {
            for (x = clogoMinX; x <= clogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (choriz_edgemask[index] && testFrame[index] < 200)
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (cvert_edgemask[index] && testFrame[index] < 200)
                {
                    if (TEST_VEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
            }
        }
    }
    else
    {
        for (y = clogoMinY; y <= clogoMaxY; y += edge_step)
        {
            for (x = clogoMinX; x <= clogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (choriz_edgemask[index])
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (cvert_edgemask[index])
                {
                    if (TEST_VEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    if (testEdges == 0)
        return(0.5);
    return (((double)goodEdges / (double)testEdges));
}

double DoubleCheckStationLogoEdge(unsigned char* testFrame)
{
    int		index;
    int		x;
    int		y;
    int		testEdges = 0;

    int goodEdges = 0;

    currentGoodEdge = 0.0;
    if (aggressive_logo_rejection == 1)
    {
        for (y = tlogoMinY; y <= tlogoMaxY; y += edge_step)
        {
            for (x = tlogoMinX; x <= tlogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (thoriz_edgemask[index])
                {
                    if (TEST_HEDGE1(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (tvert_edgemask[index])
                {
                    if (TEST_VEDGE1(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (aggressive_logo_rejection == 2)
    {
        for (y = tlogoMinY; y <= tlogoMaxY; y += edge_step)
        {
            for (x = tlogoMinX; x <= tlogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (thoriz_edgemask[index])
                {
                    if (TEST_HEDGE2(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (tvert_edgemask[index])
                {
                    if (TEST_VEDGE2(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (aggressive_logo_rejection == 3)
    {
        for (y = tlogoMinY; y <= tlogoMaxY; y += edge_step)
        {
            for (x = tlogoMinX; x <= tlogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (thoriz_edgemask[index])
                {
                    if (TEST_HEDGE3(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (tvert_edgemask[index])
                {
                    if (TEST_VEDGE3(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (aggressive_logo_rejection == 4)
    {
        for (y = tlogoMinY; y <= tlogoMaxY; y += edge_step)
        {
            for (x = tlogoMinX; x <= tlogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (thoriz_edgemask[index] && testFrame[index] < 200)
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (tvert_edgemask[index] && testFrame[index] < 200)
                {
                    if (TEST_VEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
            }
        }
    }
    else
    {
        for (y = tlogoMinY; y <= tlogoMaxY; y += edge_step)
        {
            for (x = tlogoMinX; x <= tlogoMaxX; x += edge_step)
            {
                index = y * width + x;
                if (thoriz_edgemask[index])
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (tvert_edgemask[index])
                {
                    if (TEST_VEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    if (testEdges == 0)
        return(0.5);
    return (((double)goodEdges / (double)testEdges));
}

void InitProcessLogoTest()
{
    logo_block_count = 0;
    logoTrendCounter = 0;
    frames_with_logo = 0;
    lastLogoTest = false;
    curLogoTest = false;
}


#define LOGO_SAMPLE (int)(fps * logoFreq)

bool ProcessLogoTest(int framenum_real, int curLogoTest, int close)
{



    int i;
    double s1,s2;

    if (logo_filter > 0)
    {
        if (!close)
        {

            if (framenum_real > logo_filter * 2 * LOGO_SAMPLE)
            {
                s1 = s2 = 0.0;
                for ( i = 0; i < logo_filter; i++)
                {
                    s1 += (frame[framenum_real - i * LOGO_SAMPLE - logo_filter * LOGO_SAMPLE].currentGoodEdge - logo_threshold > 0 ? 1 : -1);
                    s2 += (frame[framenum_real - i * LOGO_SAMPLE].currentGoodEdge - logo_threshold > 0 ? 1 : -1);
                }
                s1 /= logo_filter;
                s2 /= logo_filter;
                for (i = 0; i < LOGO_SAMPLE; i++)
                {
                    frame[framenum_real - logo_filter * LOGO_SAMPLE - i].logo_filter = (s1 + s2);
                }
            }
            for (i = 0; i < LOGO_SAMPLE; i++)
            {
                frame[framenum_real - i].logo_filter = 0.0;
            }

            framenum_real -= logo_filter*LOGO_SAMPLE;
            if (framenum_real < 0) framenum_real= 1;

            curLogoTest = (frame[framenum_real].logo_filter > 0.0 ? 1 : 0);
        }
        else
            curLogoTest = false;
    }

    if (curLogoTest != lastLogoTest)
    {
        if (!curLogoTest)
        {
            // Logo disappeared
            lastLogoTest = false;
            logoTrendCounter = 0;
            logo_block[logo_block_count].end = framenum_real - 1 * (int)(fps * logoFreq);
            if (logo_block[logo_block_count].end - logo_block[logo_block_count].start >
                    2*(int)(shrink_logo*fps) + (shrink_logo_tail*fps) )
            {
                logo_block[logo_block_count].end -= (int)(shrink_logo*fps) + (int)(shrink_logo_tail*fps);
                logo_block[logo_block_count].start += (int)(shrink_logo*fps);
                frames_with_logo -= 2 * (int)(fps * logoFreq) + 2*(int)(shrink_logo*fps) + (int)(shrink_logo_tail*fps);
                if (framearray)
                {
                    i = logo_block[logo_block_count].end;
                    if (i<0) i = 0;
                    for (; i < framenum_real; i++)
                        frame[i].logo_present = false;
                }
                Debug
                (3,
                 "\nEnd logo block %i\tframe %i\tLength - %s\n",
                 logo_block_count,
                 logo_block[logo_block_count].end,
                 dblSecondsToStrMinutes(F2L(logo_block[logo_block_count].end, logo_block[logo_block_count].start))
                );
                logo_block_count++;
                InitializeLogoBlockArray( logo_block_count);
            }
            else
            {
                logo_block[logo_block_count].start = -1; // else discard logo cblock
            }
        }
        else
        {
            // real change or false change?
            logoTrendCounter++;
            if (logoTrendCounter == minHitsForTrend)
            {
                lastLogoTest = true;
                logoTrendCounter = 0;
                InitializeLogoBlockArray(logo_block_count + 2);
                logo_block[logo_block_count + 1].start = -1;
                logo_block[logo_block_count].start = max(framenum_real - ((int)(fps * logoFreq) * (minHitsForTrend - 1)),0);
                frames_with_logo +=((int)(fps * logoFreq) * (minHitsForTrend - 1));
                if (framearray)
                {
                    for (i = logo_block[logo_block_count].start; i < framenum_real; i++)
                        frame[i].logo_present = true;
                }
                if (!logo_block_count)
                {
                    Debug(
                        3,
                        "\t\t\t\tStart logo cblock %i\tframe %i\n",
                        logo_block_count,
                        logo_block[logo_block_count].start
                    );
                }
                else
                {
                    Debug(
                        3,
                        "\n\t\t\t\tNonlogo Length - %s\nStart logo cblock %i\tframe %i\n",
                        dblSecondsToStrMinutes(F2L(logo_block[logo_block_count].start, logo_block[logo_block_count - 1].end)),
                        logo_block_count,
                        logo_block[logo_block_count].start
                    );
                }

            }
        }
    }
    else
    {
        logoTrendCounter = 0;
    }


    return(lastLogoTest);
}


void ResetLogoBuffers(void)
{
    newestLogoBuffer = oldestLogoBuffer = 0;
    if (logoFrameNum) {
        if (newestLogoBuffer == num_logo_buffers) newestLogoBuffer = 0; // rotates buffer
        logoFrameNum[newestLogoBuffer] = framenum_real;
        oldestLogoBuffer = 0;
    /*
         for (i = 0; i < num_logo_buffers; i++) {
              free(logoFrameBuffer[i]);
         }
         for (i = 0; i < num_logo_buffers; i++) {
              logoFrameBuffer[i] = malloc(width * height * sizeof(frame_ptr[0]));
              if (logoFrameBuffer[i] == NULL) {
                   Debug(0, "Could not allocate memory for logo frame buffer %i\n", i);
                   exit(16);
              }
         */
    }
}

void FillLogoBuffer(void)
{
    int i;
    newestLogoBuffer++;
    if (newestLogoBuffer == num_logo_buffers) newestLogoBuffer = 0; // rotates buffer
    logoFrameNum[newestLogoBuffer] = framenum_real;
    oldestLogoBuffer = 0;
    for (i = 0; i < num_logo_buffers; i++)
    {
        if (logoFrameNum[i]  && logoFrameNum[i] < logoFrameNum[oldestLogoBuffer]) oldestLogoBuffer = i;
    }

    i = min((unsigned int)logoFrameBufferSize, width * height * sizeof(frame_ptr[0]));
    memcpy(logoFrameBuffer[newestLogoBuffer], frame_ptr, i);

//	for (y = 0; y < height; y++) {
//		for (x = 0; x < width; x++) {
//			logoFrameBuffer[newestLogoBuffer][y * width + x] = frame_ptr[y * width + x];
//		}
//	}

    EdgeDetect(logoFrameBuffer[newestLogoBuffer], newestLogoBuffer);
    if ((!logoBuffersFull) && (newestLogoBuffer == num_logo_buffers - 1)) logoBuffersFull = true;
}

bool SearchForLogoEdges(void)
{
    int		i;
    int		x;
    int		y;
    double scale = ((double)height / 572) * ( (double) videowidth / 720 );
    double	logoPercentageOfScreen;
    bool	LogoIsThere;
    int		sum;
    int		tempMinX;
    int		tempMaxX;
    int		tempMinY;
    int		tempMaxY;
    int		last_non_logo_frame;
    int		logoFound = false;
    tlogoMinX = edge_radius + border;
    tlogoMaxX = videowidth - edge_radius - border;
    tlogoMinY = edge_radius + border;
    tlogoMaxY = height - edge_radius - border;
#if MULTI_EDGE_BUFFER
    memset(thoriz_edgemask, 1, width * height);
    memset(ttvert_edgemask, 1, width * height);
    for (i = 0; i < 1; i++)
    {
        for (y = border; y < height - border; y++)
        {
            for (x = border; x < videowidth - border; x++)
            {
                if (!thoriz_edgemask[y * width + x] || !horiz_edges[i][y * width + x])
                {
                    thoriz_edgemask[y * width + x] = 0;
                }

                if (!tvert_edgemask[y * width + x] || !vert_edges[i][y * width + x])
                {
                    tvert_edgemask[y * width + x] = 0;
                }
            }
        }
    }
#if 0
    for (y = border; y < height - border; y++)
    {
        for (x = border; x < videowidth - border; x++)
        {
            index = y * width + x;
            for (i = 1; i < num_logo_buffers; i++)
            {
                if (!thoriz_edgemask[index] || !horiz_edges[i][index])
                {
                    thoriz_edgemask[index] = 0;
                    break;
                }
            }
            for (i = 1; i < num_logo_buffers; i++)
            {
                if (!tvert_edgemask[index] || !vert_edges[i][index])
                {
                    tvert_edgemask[index] = 0;
                    break;
                }
            }
        }
    }
#else
    for (i = 1; i < num_logo_buffers; i++)
    {
        for (y = border; y < height - border; y++)
        {
            for (x = border; x < videowidth - border; x++)
            {
                if (!thoriz_edgemask[y * width + x] || !horiz_edges[i][y * width + x])
                {
                    thoriz_edgemask[y * width + x] = 0;
                }

                if (!tvert_edgemask[y * width + x] || !vert_edges[i][y * width + x])
                {
                    tvert_edgemask[y * width + x] = 0;
                }
            }
        }
    }
#endif
#else
    memset(thoriz_edgemask, 0, width * height);
    memset(tvert_edgemask, 0, width * height);
//	minY = (logo_at_bottom ? height/2 : edge_radius + (int)(height * borderIgnore));
//	if (framearray) minY = max(minY, frame[frame_count].minY);
//	maxY = (subtitles? height/2 : height - edge_radius - (int)(height * borderIgnore));
//	if (framearray) maxY = min(maxY, frame[frame_count].maxY);

    LOGO_X_LOOP
    {
        LOGO_Y_LOOP {
//	for (y = minY; y < maxY; y++) {
//		for (x = edge_radius + (int)(width * borderIgnore); x < videowidth - edge_radius + (int)(width * borderIgnore); x++) {
            if (hor_edgecount[y * width + x] >= num_logo_buffers * 0.95 )
            {
                thoriz_edgemask[y * width + x] = 1;
            }
            if (ver_edgecount[y * width + x] >= num_logo_buffers * 0.95 )
            {
                tvert_edgemask[y * width + x] = 1;
            }
        }
    }
#endif

    ClearEdgeMaskArea(thoriz_edgemask, tvert_edgemask);
    ClearEdgeMaskArea(tvert_edgemask, thoriz_edgemask);


    SetEdgeMaskArea(thoriz_edgemask);
    tempMinX = tlogoMinX;
    tempMaxX = tlogoMaxX;
    tempMinY = tlogoMinY;
    tempMaxY = tlogoMaxY;
    tlogoMinX = edge_radius + border;
    tlogoMaxX = videowidth - edge_radius - border;
    tlogoMinY = edge_radius + border;
    tlogoMaxY = height - edge_radius - border;
    SetEdgeMaskArea(tvert_edgemask);
    if (tempMinX < tlogoMinX) tlogoMinX = tempMinX;
    if (tempMaxX > tlogoMaxX) tlogoMaxX = tempMaxX;
    if (tempMinY < tlogoMinY) tlogoMinY = tempMinY;
    if (tempMaxY > tlogoMaxY) tlogoMaxY = tempMaxY;
    edgemask_filled = 1;
    logoPercentageOfScreen = (double)((tlogoMaxY - tlogoMinY) * (tlogoMaxX - tlogoMinX)) / (double)(height * width);
    if (logoPercentageOfScreen > logo_max_percentage_of_screen)
    {
//			Debug(
//				3,
//				"Reducing logo search area!\tPercentage of screen - %.2f%% TOO BIG.\n",
//				logoPercentageOfScreen * 100
//			);

//        if (tempMinX > tlogoMinX+50) tlogoMinX = tempMinX;
//        if (tempMaxX < tlogoMaxX-50) tlogoMaxX = tempMaxX;
//        if (tempMinY > tlogoMinY+50) tlogoMinY = tempMinY;
//        if (tempMaxY < tlogoMaxY-50) tlogoMaxY = tempMaxY;
    }

    i = CountEdgePixels();
//printf("Edges=%d\n",i);
//	if (i > 350/(lowres+1)/(edge_step)) {
    if ( i > 150 * scale /edge_step)
    {
        logoPercentageOfScreen = (double)((tlogoMaxY - tlogoMinY) * (tlogoMaxX - tlogoMinX)) / (double)(height * width);
        if (i > 40000 || logoPercentageOfScreen > logo_max_percentage_of_screen)
        {
            Debug(
                3,
                "Edge count - %i\tPercentage of screen - %.2f%% TOO BIG, CAN'T BE A LOGO.\n",
                i,
                logoPercentageOfScreen * 100
            );
//			logoInfoAvailable = false;
        }
        else
        {
            Debug(3, "Edge count - %i\tPercentage of screen - %.2f%%, Check: %i\n", i, logoPercentageOfScreen * 100,doublCheckLogoCount);
//			logoInfoAvailable = true;
            logoFound = true;
        }
    }
    else
        Debug(3, "Not enough edge count - %i\n", i);


    if (logoFound)
    {
        doublCheckLogoCount++;
        Debug(3, "Double checking - %i\n", doublCheckLogoCount );

        if (doublCheckLogoCount > 1)
        {
            // Final check done, found
        }
        else
            logoFound = false;
    }
    else
    {
        doublCheckLogoCount = 0;
    }


    sum = 0;
    oldestLogoBuffer = 0;
    for (i = 0; i < num_logo_buffers; i++)
    {
        if (logoFrameNum[i]  && logoFrameNum[i] < logoFrameNum[oldestLogoBuffer]) oldestLogoBuffer = i;
    }
    last_non_logo_frame = logoFrameNum[oldestLogoBuffer];
    if (logoFound)
    {
        Debug(3, "Doublechecking frames %i to %i for logo.\n", logoFrameNum[oldestLogoBuffer], logoFrameNum[newestLogoBuffer]);
        for (i = 0; i < num_logo_buffers; i++)
        {
            currentGoodEdge = DoubleCheckStationLogoEdge(logoFrameBuffer[i]);
            LogoIsThere = (currentGoodEdge > logo_threshold);

            for (x = logoFrameNum[i]; x < logoFrameNum[i] + (int)( logoFreq * fps ); x++)
            {
                frame[x].currentGoodEdge = currentGoodEdge;
                frame[x].logo_present = LogoIsThere;
                if (!LogoIsThere)
                {
                    if (x > last_non_logo_frame)
                        last_non_logo_frame = x;
                }
            }
            if (LogoIsThere)
            {
//				Debug(7, "Logo present in frame %i.\n", logoFrameNum[i]);
                sum++;
            }
            else
            {
                Debug(7, "Logo not present in frame %i.\n", logoFrameNum[i]);
            }
        }
    }


    if (logoFound && (sum >= (int)(num_logo_buffers * .9)))
    {

        clogoMinX = tlogoMinX;
        clogoMaxX = tlogoMaxX;
        clogoMinY = tlogoMinY;
        clogoMaxY = tlogoMaxY;
        memcpy(choriz_edgemask, thoriz_edgemask, width * height);
        memcpy(cvert_edgemask, tvert_edgemask, width * height);


        logoTrendCounter = num_logo_buffers;
        lastLogoTest = true;
        curLogoTest = true;

        logo_block[logo_block_count].start = last_non_logo_frame+1;
        DumpEdgeMasks();
//		DumpEdgeMask(choriz_edgemask, HORIZ);
//		DumpEdgeMask(cvert_edgemask, VERT);
//		for (i = 0; i < num_logo_buffers; i++) {
#if MULTI_EDGE_BUFFER
//			free(vert_edges[i]);
//			free(horiz_edges[i]);
#endif
//			free(logoFrameBuffer[i]);
//		}
#if MULTI_EDGE_BUFFER
//		free(vert_edges);
//		vert_edges = NULL
//		free(horiz_edges);
//		horiz_edges = NULL;
#else
//		free(horiz_count);
//		horiz_count = NULL;
//		free(vert_count);
//		vert_count = NULL;
#endif
//		free(logoFrameBuffer);
//		logoFrameBuffer = NULL;
        InitScanLines();
        InitHasLogo();

        logoInfoAvailable = true; //xxxxxxx
    }
    else
    {
//		logoInfoAvailable = false; //xxxxxxx
        currentGoodEdge = 0.0;
    }

    if (!logoInfoAvailable && startOverAfterLogoInfoAvail && (framenum_real > (int)(giveUpOnLogoSearch * fps)))
    {
        Debug(1, "No logo was found after %i frames.\nGiving up", framenum_real);
        commDetectMethod -= LOGO;
    }
    if (added_recording > 0)
        giveUpOnLogoSearch += added_recording * 60;

    if (logoInfoAvailable && startOverAfterLogoInfoAvail)
    {
        Debug(3, "Logo found at frame %i\tlogoMinX=%i\tlogoMaxX=%i\tlogoMinY=%i\tlogoMaxY=%i\n", framenum_real, clogoMinX, clogoMaxX, clogoMinY, clogoMaxY);
        SaveLogoMaskData();
        Debug(3, "******************* End of Logo Processing ***************\n");
        return false;
    }

    return true;
}


#define MAX_SEARCH_FRACTION 0.02

int ClearEdgeMaskArea(unsigned char* temp, unsigned char* test)
{
    int x;
    int y;
    int count;
    int valid = 0;
    int offset;
    int ix,iy;

    LOGO_X_LOOP
    {
        LOGO_Y_LOOP
        {
            count = 0;
            if (temp[y * width + x] == 1)
            {
                if (test[y * width + x] == 1)
//					goto found;
                    count++;

                for (offset = edge_step; offset < (int) (MAX_SEARCH_FRACTION * width); offset += edge_step)
                {
                    iy = min(y+offset,height-1);
                    for (ix= max(x-offset,0); ix <= min(x+offset, width-1); ix += edge_step)
                        if (test[iy * width + ix] == 1)
//							goto found;
                            count++;

                    iy = max(y-offset,0);
                    for (ix= max(x-offset,0); ix <= min(x+offset, width-1); ix += edge_step)
                        if (test[iy * width + ix] == 1)
//							goto found;
                            count++;

                    ix = min(x+offset, width-1);
                    for (iy= max(y-offset+edge_step,0); iy <=  min(y+offset-edge_step,height-1); iy += edge_step)
                        if (test[iy * width + ix] == 1)
//							goto found;
                            count++;

                    ix = max(x-offset,0);
                    for (iy= max(y-offset+edge_step,0); iy <=  min(y+offset-edge_step,height-1); iy += edge_step)
                        if (test[iy * width + ix] == 1)
//							goto found;
                            count++;
                    if (count >= edge_weight)
                        goto found;
                }
                temp[y * width + x] = 0;
                continue;
found:
                valid++;
            }
        }
    }
    return(valid);
}

void SetEdgeMaskArea(unsigned char* temp)
{
    int x;
    int y;
    tlogoMinX = videowidth - 1;
    tlogoMaxX = 0;
    tlogoMinY = height - 1;
    tlogoMaxY = 0;
    LOGO_X_LOOP
//    for (y = (logo_at_bottom ? height/2 : border + edge_radius); y < (subtitles? height/2 : height - border - edge_radius); y++)
    {
        LOGO_Y_LOOP
//        for (x = border+edge_radius; x < videowidth - border - edge_radius; x++)
        {
            if (temp[y * width + x] == 1)
            {
                if (x - LOGOBORDER < tlogoMinX) tlogoMinX = x - LOGOBORDER;
                if (y - LOGOBORDER < tlogoMinY) tlogoMinY = y - LOGOBORDER;
                if (x + LOGOBORDER > tlogoMaxX) tlogoMaxX = x + LOGOBORDER;
                if (y + LOGOBORDER > tlogoMaxY) tlogoMaxY = y + LOGOBORDER;
            }
        }
    }

    if (tlogoMinX < edge_radius) tlogoMinX = edge_radius;
    if (tlogoMaxX > (videowidth - edge_radius)) tlogoMaxX = (videowidth - edge_radius);
    if (tlogoMinY < edge_radius) tlogoMinY = edge_radius;
    if (tlogoMaxY > (height - edge_radius)) tlogoMaxY = (height - edge_radius);
}

int CountEdgePixels(void)
{
    int x;
    int y;
    int count = 0;
    int hcount = 0;
    int vcount = 0;
    for (y = tlogoMinY; y <= tlogoMaxY; y++)
    {
        for (x = tlogoMinX; x <= tlogoMaxX; x++)
        {
            if (thoriz_edgemask[y * width + x]) hcount++;
            if (tvert_edgemask[y * width + x]) vcount++;
        }
    }
    count = hcount + vcount;
//    if (count>0)
//        Debug(1, "\nFrame[%d] edgecount=%d",framenum_real, count);
//	printf("%6d %6d\n",hcount, vcount);
    //if ((hcount < 50 * scale / edge_step) || (vcount < 50 * scale /edge_step )) count = 0;
    return (count);
}

void DumpEdgeMask(unsigned char* buffer, int direction)
{
    int x;
    int y;
    char outbuf[MAXWIDTH+1];
    switch (direction)
    {
    case HORIZ:
        Debug(1, "\nHorizontal Logo Mask \n     ");
        break;

    case VERT:
        Debug(1, "\nVertical Logo Mask \n     ");
        break;

    case DIAG1:
        Debug(1, "\nDiagonal 1 Logo Mask \n     ");
        break;

    case DIAG2:
        Debug(1, "\nDiagonal 2 Logo Mask \n     ");
        break;
    }

    for (x = clogoMinX; x <= clogoMaxX; x++)
    {
        outbuf[x-clogoMinX] = '0'+ (x % 10);
    }
    outbuf[x-clogoMinX] = 0;
    Debug(1, "%s\n",outbuf);


    Debug(1, "\n");
    for (y = clogoMinY; y <= clogoMaxY; y++)
    {
        Debug(1, "%3d: ", y);
        for (x = clogoMinX; x <= clogoMaxX; x++)
        {
            switch (buffer[y * width + x])
            {
            case 0:
                outbuf[x-clogoMinX] = ' ';
                break;

            case 1:
                outbuf[x-clogoMinX] = '*';
                break;
            }
        }
        outbuf[x-clogoMinX] = 0;
        Debug(1, "%s\n",outbuf);

    }
}

void DumpEdgeMasks(void)
{
    int x;
    int y;
    char outbuf[MAXWIDTH+1];

    for (x = clogoMinX; x <= clogoMaxX; x++)
    {
        outbuf[x-clogoMinX] = '0'+ (x % 10);
    }
    outbuf[x-clogoMinX] = 0;
    Debug(1, "%s\n",outbuf);

    for (y = clogoMinY; y <= clogoMaxY; y++)
    {
        Debug(1, "%3d: ", y);
        for (x = clogoMinX; x <= clogoMaxX; x++)
        {
            switch (choriz_edgemask[y * width + x])
            {
            case 0:
                if (cvert_edgemask[y * width + x] == 1)
                    outbuf[x-clogoMinX] =  '-';
                else
                    outbuf[x-clogoMinX] =  ' ';
                break;

            case 1:
                if (cvert_edgemask[y * width + x] == 1)
                    outbuf[x-clogoMinX] =  '+';
                else
                    outbuf[x-clogoMinX] =  '|';
                break;
            }
        }
        outbuf[x-clogoMinX] = 0;
        Debug(1, "%s\n",outbuf);
    }
}

bool CheckFramesForLogo(int start, int end)
{
    int		i;
#ifdef OLD_LIVE_TV
    int		j;
    for (i = start; i <= end; i++)
    {
        for (j = 0; j < logo_block_count; j++)
        {
            if (i > logo_block[j].start && i < logo_block[j].end)
            {
                return (!reverseLogoLogic);
            }
        }
    }

    return (reverseLogoLogic);
#else
    double sum = 0.0;
    for (i = start; i <= end; i++)
        sum += (frame[i].currentGoodEdge > logo_threshold ? 1 : 0);

    sum = sum / (end - start + 1);
    if (sum > logo_percentage_threshold)
        return(true);
    return(false);

#endif

}

double CalculateLogoFraction(int start, int end)
{
    int		i,j;
    int		count=0;
    j = 0;
    for (i = start; i <= end; i++)
    {
        while (j < logo_block_count && i > logo_block[j].end) j++;
        if (j < logo_block_count && i >= logo_block[j].start && i <= logo_block[j].end )
            count++;
    }
    if (reverseLogoLogic)
        return (1.0 - (double) count / (double)(end - start + 1));
    return ((double) count / (double)(end - start + 1));
}

bool CheckFrameForLogo(int i)
{
    int		j=0;
    while (j < logo_block_count && i > logo_block[j].end) j++;
    if (j < logo_block_count && i <= logo_block[j].end && i >= logo_block[j].start )
    {
        return(!reverseLogoLogic);
    }
    return (reverseLogoLogic);
}



char CheckFramesForCommercial(int start, int end)
{
    int		i;
    if (start >= end )
        return ('0');						// Too short to decide
    i = 0;
    while (i <= commercial_count && start > commercial[i].end_frame)
        i++;
    if (i <= commercial_count)  			// Now start <= commercial[i].end_frame
    {
        if (end < commercial[i].start_frame)
            return('+');
        if (start < commercial[i].start_frame)
            return('0');
        return('-');
    }
    return('+');
}

char CheckFramesForReffer(int start, int end)
{
    int		i;
    if (reffer_count < 0)
        return(' ');
    if (start >= end )
        return ('0');						// Too short to decide
    i = 0;
    while (i <= reffer_count &&  reffer[i].end_frame < start + fps)
        i++;
    if (i <= reffer_count)  			// Now start <= reffer[i].end_frame
    {
        if (reffer[i].start_frame < start + fps)
            return('-');
        if (reffer[i].start_frame > end - fps)
            return('+');
        if ( reffer[i].start_frame < end + fps)
            return('0');
        return('-');
    }
    return('+');
}

void SaveLogoMaskData(void)
{
    FILE*	logo_file;
    int		x;
    int		y;
    logo_file = myfopen(logofilename, "w");
    if (!logo_file)
    {
        fprintf(stderr, "%s - could not create file %s\n", strerror(errno), logofilename);
        Debug(1, "%s - could not create file %s\n", strerror(errno), logofilename);
        if(startOverAfterLogoInfoAvail)
            exit(7);
    }

    fprintf(logo_file, "logoMinX=%i\n", clogoMinX);
    fprintf(logo_file, "logoMaxX=%i\n", clogoMaxX);
    fprintf(logo_file, "logoMinY=%i\n", clogoMinY);
    fprintf(logo_file, "logoMaxY=%i\n", clogoMaxY);
    fprintf(logo_file, "picWidth=%i\n", width);
    fprintf(logo_file, "picHeight=%i\n", height);
    if (1)
    {
        fprintf(logo_file, "\nCombined Logo Mask\n");
        fprintf(logo_file, "\202\n");
        for (y = clogoMinY; y <= clogoMaxY; y++)
        {
            for (x = clogoMinX; x <= clogoMaxX; x++)
            {
                switch (choriz_edgemask[y * width + x])
                {
                case 0:
                    if (cvert_edgemask[y * width + x] == 1)
                        fprintf(logo_file, "-");
                    else
                        fprintf(logo_file, " ");
                    break;

                case 1:
                    if (cvert_edgemask[y * width + x] == 1)
                        fprintf(logo_file, "+");
                    else
                        fprintf(logo_file, "|");
                    break;
                }
            }

            fprintf(logo_file, "\n");
        }

    }
    else
    {
        fprintf(logo_file, "\nHorizonatal Logo Mask\n");
        fprintf(logo_file, "\200\n");
        for (y = clogoMinY; y <= clogoMaxY; y++)
        {
            for (x = clogoMinX; x <= clogoMaxX; x++)
            {
                switch (choriz_edgemask[y * width + x])
                {
                case 0:
                    fprintf(logo_file, " ");
                    break;

                case 1:
                    fprintf(logo_file, "|");
                    break;
                }
            }

            fprintf(logo_file, "\n");
        }

        fprintf(logo_file, "\nVertical Logo Mask\n");
        fprintf(logo_file, "\201\n");
        for (y = clogoMinY; y <= clogoMaxY; y++)
        {
            for (x = clogoMinX; x <= clogoMaxX; x++)
            {
                switch (cvert_edgemask[y * width + x])
                {
                case 0:
                    fprintf(logo_file, " ");
                    break;

                case 1:
                    fprintf(logo_file, "-");
                    break;
                }
            }

            fprintf(logo_file, "\n");
        }
    }

    fclose(logo_file);
}

void LoadLogoMaskData(void)
{
    FILE*	logo_file = NULL;
    FILE*	txt_file;
    int		x;
    int		y;
    double	tmp;
    char	temp;
    char	data[2000];
    char	*ptr = NULL;
    long	tmpLong = 0;
    size_t	len = 0;

    logo_file = myfopen(logofilename, "r");
    if (logo_file)
    {
        Debug(1, "Using %s for logo data.\n", logofilename);
        len = fread(data, 1, 1999, logo_file);
        fclose(logo_file);
        data[len] = '\0';
        if ((tmp = FindNumber(data, "picWidth=", (double) width)) > -1) videowidth = width = (int)tmp;
        if ((tmp = FindNumber(data, "picHeight=", (double) height)) > -1) height = (int)tmp;
        if ((tmp = FindNumber(data, "logoMinX=", (double) clogoMinX)) > -1) clogoMinX = (int)tmp;
        if ((tmp = FindNumber(data, "logoMaxX=", (double) clogoMaxX)) > -1) clogoMaxX = (int)tmp;
        if ((tmp = FindNumber(data, "logoMinY=", (double) clogoMinY)) > -1) clogoMinY = (int)tmp;
        if ((tmp = FindNumber(data, "logoMaxY=", (double) clogoMaxY)) > -1) clogoMaxY = (int)tmp;
    }
    else
    {
        Debug(0, "Could not find the logo file.\n");
        logoInfoAvailable = false;
        return;
    }

    logo_file = myfopen(logofilename, "r");
    /*
    	choriz_edgemask = malloc(width * height * sizeof(unsigned char));
    	if (choriz_edgemask == NULL) {
    		Debug(0, "Could not allocate memory for horizontal edgemask\n");
    		exit(8);
    	}

    	cvert_edgemask = malloc(width * height * sizeof(unsigned char));
    	if (cvert_edgemask == NULL) {
    		Debug(0, "Could not allocate memory for vertical edgemask\n");
    		exit(9);
    	}
    	memset(choriz_edgemask, 0, width * height);
    	memset(cvert_edgemask, 0, width * height);
    */
    do
    {
        temp = getc(logo_file);
    }
    while ((temp != '\200') && !feof(logo_file));
    for (y = clogoMinY; y <= clogoMaxY; y++)
    {
        for (x = clogoMinX; x <= clogoMaxX; x++)
        {
            temp = getc(logo_file);
            if (temp == '\n') temp = getc(logo_file);				// If a carrage return was retrieved, get the next character
            switch (temp)
            {
            case ' ':
                choriz_edgemask[y * width + x] = 0;
                break;

            case '|':
                choriz_edgemask[y * width + x] = 1;
                break;
            }
        }
    }

    fclose(logo_file);
    logo_file = myfopen(logofilename, "r");
    do
    {
        temp = getc(logo_file);
    }
    while ((temp != '\201') && !feof(logo_file));
    for (y = clogoMinY; y <= clogoMaxY; y++)
    {
        for (x = clogoMinX; x <= clogoMaxX; x++)
        {
            temp = getc(logo_file);
            if (temp == '\n') temp = getc(logo_file);				// If a carrage return was retrieved, get the next character
            switch (temp)
            {
            case ' ':
                cvert_edgemask[y * width + x] = 0;
                break;

            case '-':
                cvert_edgemask[y * width + x] = 1;
                break;
            }
        }
    }
    fclose(logo_file);

    logo_file = myfopen(logofilename, "r");
    do
    {
        temp = getc(logo_file);
    }
    while ((temp != '\202') && !feof(logo_file));
    if (!feof(logo_file))
    {
        for (y = clogoMinY; y <= clogoMaxY; y++)
        {
            for (x = clogoMinX; x <= clogoMaxX; x++)
            {
                temp = getc(logo_file);
                if (temp == '\n') temp = getc(logo_file);				// If a carrage return was retrieved, get the next character
                switch (temp)
                {
                case ' ':
                    choriz_edgemask[y * width + x] = 0;
                    cvert_edgemask[y * width + x] = 0;
                    break;

                case '-':
                    choriz_edgemask[y * width + x] = 0;
                    cvert_edgemask[y * width + x] = 1;
                    break;

                case '|':
                    choriz_edgemask[y * width + x] = 1;
                    cvert_edgemask[y * width + x] = 0;
                    break;

                case '+':
                    choriz_edgemask[y * width + x] = 1;
                    cvert_edgemask[y * width + x] = 1;
                    break;

                }
            }
        }
    }
    fclose(logo_file);


    logoInfoAvailable = true;
    startOverAfterLogoInfoAvail = true; // prevent continuous searching for logo when a logo file is specified
    secondLogoSearch = true;
    InitScanLines();
    InitHasLogo();
    isSecondPass = true;
    if (!loadingCSV)
    {
//		DumpEdgeMask(choriz_edgemask, HORIZ);
//		DumpEdgeMask(cvert_edgemask, VERT);
        DumpEdgeMasks();
    }
    memset(data, 0, sizeof(data));
    _flushall();
    if (output_default)
    {
        txt_file = myfopen(out_filename, "r");
        if (!txt_file)
        {
            Sleep(50L);
            txt_file = myfopen(out_filename, "r");
            if (!txt_file)
            {
                Debug(0, "ERROR reading from %s\n", out_filename);
                isSecondPass = false;
                return;
            }
        }


        if(fseek( txt_file, 0L, SEEK_SET ))
        {
            Debug(0, "ERROR SEEKING\n");
        }


        while (fgets(data, 1999, txt_file) != NULL)
        {
            if (strstr(data, "FILE PROCESSING COMPLETE") != NULL)
            {
                lastFrame = 0;
                break;
            }
            ptr = strchr(data, '\t');
            if (ptr != NULL)
            {
                ptr++;
                tmpLong = strtol(ptr, NULL, 10);
                if (tmpLong > lastFrame)
                {
                    lastFrame = tmpLong;
                }
            }
        }
        fclose(txt_file);
    }
    Debug(10, "The last frame found in %s was %i\n", out_filename, lastFrame);
}

