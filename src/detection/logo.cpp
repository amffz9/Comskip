#include "exit_requested.h"
#include "legacy_detection.h"
#include "image_geometry.h"

void PrintLogoFrameGroups(RecordingContext& context)
{
    int		i,l;
    double  cl;
    int		f,t;
    int		count = 0;

    Debug(context, 2, "\nLogos detected on the following frames\n--------------------------------------\n");
    count = 0;
    for (i = 0; i < context.state.logo_block_count; i++)
    {
        f = FindBlock(context, context.state.logo_block[i].start);
        t = FindBlock(context, context.state.logo_block[i].end-2);
        if (f<0) f = 0;
        if (t<0) t = 0;
        if (t < 0)
        {
            Debug (context, 2, "Panic\n");
            break;
        }
        if (f < 0)
        {
            Debug (context, 2, "Panic\n");
            break;
        }
        Debug(context,
            2,
            "Logo start - %6i\tend - %6i\tlength - %s\tbefore:%.1f s\t after:%.1f s\n",
            context.state.logo_block[i].start,
            context.state.logo_block[i].end,
            dblSecondsToStrMinutes(context, F2L(context.state.logo_block[i].end, context.state.logo_block[i].start)),
            F2L(context.state.logo_block[i].start, context.state.cblock[f].f_start),
            F2L(context.state.cblock[t].f_end, context.state.logo_block[i].end)
        );

        count += context.state.logo_block[i].end - context.state.logo_block[i].start + 1;

    }
    for (i = 0; i < context.state.logo_block_count-1; i++)
    {
        f = context.state.logo_block[i].end;
        t = context.state.logo_block[i+1].start;
        if (context.state.max_logo_gap < F2L(t,f))
            context.state.max_logo_gap = F2L(t,f);
        f = FindBlock(context, context.state.logo_block[i].end);
        t = FindBlock(context, context.state.logo_block[i+1].start);
        for (l = f+1; l < t; l++)
        {
            if (context.state.max_nonlogo_block_length < context.state.cblock[l].length)
                context.state.max_nonlogo_block_length = context.state.cblock[l].length;
        }
    }
    for (i = 0; i < context.state.logo_block_count-1; i++)
    {
        f = FindBlock(context, context.state.logo_block[i].start);
        t = FindBlock(context, context.state.logo_block[i].end);
        if (F2L(context.state.logo_block[i].end, context.state.logo_block[i].start) > context.state.max_nonlogo_block_length )
        {
            cl = F2L(context.state.cblock[f].f_end, context.state.logo_block[i].start);
            if (cl < context.state.cblock[f].length / 10 )
            {
                if (cl > context.state.logo_overshoot )
                    context.state.logo_overshoot = cl;
            }
            cl = F2L(context.state.logo_block[i].end, context.state.cblock[t].f_start);
            if (cl < context.state.cblock[t].length / 10 )
            {
                if (cl > context.state.logo_overshoot)
                    context.state.logo_overshoot = cl;
            }
        }
    }
    if (context.state.logo_overshoot > 0)
        context.state.logo_overshoot = context.state.logo_overshoot + 1 + context.settings.shrink_logo;
    else
        context.state.logo_overshoot = context.settings.shrink_logo;
}

void PrintCCBlocks(RecordingContext& context)
{
    int i, j;
    Debug(context, 2, "Combining CC Blocks...\n");
    for (i = context.state.cc_block_count - 1; i > 0; i--)
    {
        if (F2L(context.state.cc_block[i].end_frame, context.state.cc_block[i].start_frame) < 1.0)
        {
            Debug(context,
                4,
                "Removing cc cblock %i because the length is %.2f.\n",
                i,
                F2L(context.state.cc_block[i].end_frame, context.state.cc_block[i].start_frame)
            );
            for (j = i; j < context.state.cc_block_count - 1; j++)
            {
                context.state.cc_block[j].start_frame = context.state.cc_block[j + 1].start_frame;
                context.state.cc_block[j].end_frame = context.state.cc_block[j + 1].end_frame;
                context.state.cc_block[j].type = context.state.cc_block[j + 1].type;
            }

            context.state.cc_block_count--;
        }
    }

    Debug(context, 2, "CC's detected on the following frames - %i total blocks\n--------------------------------------\n", context.state.cc_block_count);
    Debug(context,
        2,
        " 0 - CC start - %6i\tend - %6i\ttype - %s",
        context.state.cc_block[0].start_frame,
        context.state.cc_block[0].end_frame,
        CCTypeToStr(context, context.state.cc_block[0].type)
    );
    Debug(context, 2, "\tlength - %s\n", dblSecondsToStrMinutes(context, F2L(context.state.cc_block[0].end_frame, context.state.cc_block[0].start_frame)));
    context.state.cc_count[context.state.cc_block[0].type] += context.state.cc_block[0].end_frame - context.state.cc_block[0].start_frame + 1;

    for (i = 1; i < context.state.cc_block_count; i++)
    {
        Debug(context,
            2,
            "%2i - CC start - %6i\tend - %6i\ttype - %s",
            i,
            context.state.cc_block[i].start_frame,
            context.state.cc_block[i].end_frame,
            CCTypeToStr(context, context.state.cc_block[i].type)
        );
        Debug(context, 2, "\tlength - %s\n", dblSecondsToStrMinutes(context, F2L(context.state.cc_block[i].end_frame, context.state.cc_block[i].start_frame)));
        context.state.cc_count[context.state.cc_block[i].type] += context.state.cc_block[i].end_frame - context.state.cc_block[i].start_frame + 1;
    }

    Debug(context, 2, "\nCaption sums\n---------------------------\n");
    Debug(context,
        2,
        "Pop on captions:   %6i:%5.2f - %s\n",
        context.state.cc_count[POPON],
        ((double)context.state.cc_count[POPON] / (double)context.state.framesprocessed) * 100.0,
        dblSecondsToStrMinutes(context, context.state.cc_count[POPON] / context.settings.fps)
    );
    Debug(context,
        2,
        "Roll up captions:  %6i:%5.2f - %s\n",
        context.state.cc_count[ROLLUP],
        ((double)context.state.cc_count[ROLLUP] / (double)context.state.framesprocessed) * 100.0,
        dblSecondsToStrMinutes(context, context.state.cc_count[ROLLUP] / context.settings.fps)
    );
    Debug(context,
        2,
        "Paint on captions: %6i:%5.2f - %s\n",
        context.state.cc_count[PAINTON],
        ((double)context.state.cc_count[PAINTON] / (double)context.state.framesprocessed) * 100.0,
        dblSecondsToStrMinutes(context, context.state.cc_count[PAINTON] / context.settings.fps)
    );
    Debug(context,
        2,
        "No captions:       %6i:%5.2f - %s\n",
        context.state.cc_count[NONE],
        ((double)context.state.cc_count[NONE] / (double)context.state.framesprocessed) * 100.0,
        dblSecondsToStrMinutes(context, context.state.cc_count[NONE] / context.settings.fps)
    );
    for (i = 0; i <= 4; i++)
    {
        if (context.state.cc_count[i] > context.state.cc_count[context.state.most_cc_type])
        {
            context.state.most_cc_type = i;
        }
    }

    Debug(context, 2, "The %s type of closed captions were determined to be the most common.\n", CCTypeToStr(context, context.state.most_cc_type));
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

#define TEST_HEDGE1(FRAME,X,Y)	(abs(FRAME[(Y) * context.state.width + (X) - context.settings.edge_radius]   - FRAME[(Y) * context.state.width + (X) + context.settings.edge_radius]  ) >= context.settings.edge_level_threshold)
#define TEST_VEDGE1(FRAME,X,Y)	(abs(FRAME[((Y) - context.settings.edge_radius) * context.state.width + (X)] - FRAME[((Y) + context.settings.edge_radius) * context.state.width + (X)]) >= context.settings.edge_level_threshold)

#define TEST_HEDGE0(FRAME,X,Y)  (abs(FRAME[(Y) * context.state.width + (X) - context.settings.edge_radius]   - FRAME[(Y) * context.state.width + (X)]  ) >= context.settings.edge_level_threshold) || \
                                (abs(FRAME[(Y) * context.state.width + (X) + context.settings.edge_radius]   - FRAME[(Y) * context.state.width + (X)]  ) >= context.settings.edge_level_threshold)

#define TEST_VEDGE0(FRAME,X,Y)	(abs(FRAME[((Y) - context.settings.edge_radius) * context.state.width + (X)] - FRAME[((Y)) * context.state.width + (X)]) >= context.settings.edge_level_threshold) || \
                                (abs(FRAME[((Y) + context.settings.edge_radius) * context.state.width + (X)] - FRAME[((Y)) * context.state.width + (X)]) >= context.settings.edge_level_threshold)

#define TEST_HEDGE2(FRAME,X,Y)  (abs((FRAME[(Y) * context.state.width + (X) - context.settings.edge_radius - 1] + FRAME[(Y) * context.state.width + (X) - context.settings.edge_radius] + FRAME[(Y) * context.state.width + (X) - context.settings.edge_radius + 1]) - \
                                     (FRAME[(Y) * context.state.width + (X) + context.settings.edge_radius - 1] + FRAME[(Y) * context.state.width + (X) + context.settings.edge_radius] + FRAME[(Y) * context.state.width + (X) + context.settings.edge_radius + 1])   )/3 >= context.settings.edge_level_threshold)

#define TEST_VEDGE2(FRAME,X,Y)	(abs((FRAME[((Y) - context.settings.edge_radius - 1) * context.state.width + (X)] + FRAME[((Y) - context.settings.edge_radius) * context.state.width + (X)] + FRAME[((Y) - context.settings.edge_radius + 1) * context.state.width + (X)]) - \
                                     (FRAME[((Y) + context.settings.edge_radius - 1) * context.state.width + (X)] + FRAME[((Y) + context.settings.edge_radius) * context.state.width + (X)] + FRAME[((Y) + context.settings.edge_radius + 1) * context.state.width + (X)])   )/3 >= context.settings.edge_level_threshold)


#define TEST_HEDGE3(FRAME,X,Y)	(abs((\
FRAME[((Y)-context.settings.edge_radius)*context.state.width+(X)-context.settings.edge_radius]-FRAME[((Y)-context.settings.edge_radius)*context.state.width+(X)+context.settings.edge_radius] +\
FRAME[((Y)            )*context.state.width+(X)-context.settings.edge_radius]-FRAME[((Y)            )*context.state.width+(X)+context.settings.edge_radius] +\
FRAME[((Y)+context.settings.edge_radius)*context.state.width+(X)-context.settings.edge_radius]-FRAME[((Y)+context.settings.edge_radius)*context.state.width+(X)+context.settings.edge_radius])\
) >= context.settings.edge_level_threshold)

#define TEST_VEDGE3(FRAME,X,Y)	(abs((\
FRAME[((Y)-context.settings.edge_radius)*context.state.width+(X)-context.settings.edge_radius]-FRAME[((Y)+context.settings.edge_radius)*context.state.width+(X)-context.settings.edge_radius] +\
FRAME[((Y)-context.settings.edge_radius)*context.state.width+(X)            ]-FRAME[((Y)+context.settings.edge_radius)*context.state.width+(X)            ] +\
FRAME[((Y)-context.settings.edge_radius)*context.state.width+(X)+context.settings.edge_radius]-FRAME[((Y)+context.settings.edge_radius)*context.state.width+(X)+context.settings.edge_radius])\
) >= context.settings.edge_level_threshold)


#define AR_DIST	20


void EdgeDetect(RecordingContext& context, unsigned char* frame_ptr, int maskNumber)
{
    int				x;
    int				y;
    //	unsigned char	temp[MAXWIDTH * MAXHEIGHT];
//	memset(for (i = 0; i <= (width * height); i++) temp[i] = 0;
    context.state.hedge_count = 0;
    context.state.vedge_count = 0;
#ifdef MAXMIN_LOGO_SEARCH
    if (maskNumber == 0)
    {
        memset(max_br, 0, sizeof(max_br));
        memset(min_br, 255, sizeof(max_br));
    }
    for (y = (logo_at_bottom ? context.state.height/2 : context.settings.edge_radius + (int)(context.state.height * borderIgnore)); y < (subtitles? context.state.height/2 : (context.state.height - context.settings.edge_radius - (int)(context.state.height * borderIgnore))); y++)
    {
        for (x = max(context.settings.edge_radius + (int)(context.state.width * borderIgnore), minX+AR_DIST); x < min((context.state.width - context.settings.edge_radius - (int)(context.state.width * borderIgnore)),maxX-AR_DIST); x++)
        {
            herePixel = frame_ptr[y * context.state.width + x];
            if (herePixel < min_br[y * context.state.width + x])
                min_br[y * context.state.width + x] = herePixel;
            if (herePixel > max_br[y * context.state.width + x])
                max_br[y * context.state.width + x] = herePixel;
        }
    }
#endif
#if MULTI_EDGE_BUFFER
    memset(horiz_edges[maskNumber], 0, context.state.width * context.state.height);
    memset(vert_edges[maskNumber], 0, context.state.width * context.state.height);
    for (y = (logo_at_bottom ? context.state.height/2 : context.settings.edge_radius + (int)(context.state.height * borderIgnore)); y < (subtitles? context.state.height/2 : (context.state.height - context.settings.edge_radius - (int)(context.state.height * borderIgnore))); y++)
    {
        for (x = max(context.settings.edge_radius + (int)(context.state.width * borderIgnore), minX+AR_DIST); x < min((context.state.width - context.settings.edge_radius - (int)(context.state.width * borderIgnore)),maxX-AR_DIST); x++)
        {
            herePixel = frame_ptr[y * context.state.width + x];
            if ((abs(frame_ptr[y * context.state.width + (x - context.settings.edge_radius)] - herePixel) >= context.settings.edge_level_threshold) ||
                    (abs(frame_ptr[y * context.state.width + (x + context.settings.edge_radius)] - herePixel) >= context.settings.edge_level_threshold))
            {
                horiz_edges[maskNumber][y * context.state.width + x] = 1;
            }

            if ((abs(frame_ptr[(y - context.settings.edge_radius) * context.state.width + x] - herePixel) >= context.settings.edge_level_threshold) ||
                    (abs(frame_ptr[(y + context.settings.edge_radius) * context.state.width + x] - herePixel) >= context.settings.edge_level_threshold))
            {
                vert_edges[maskNumber][y * context.state.width + x] = 1;
            }
        }
    }
#else
    if (context.settings.aggressive_logo_rejection==1)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if (TEST_HEDGE1(frame_ptr,x,y))
                {
                    if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.hor_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.hor_edgecount[y * context.state.width + x] = 0;
                if (TEST_VEDGE1(frame_ptr,x,y))
                {
                    if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.ver_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.ver_edgecount[y * context.state.width + x] = 0;
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection==2)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if (TEST_HEDGE2(frame_ptr,x,y))
                {
                    if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.hor_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.hor_edgecount[y * context.state.width + x] = 0;
                if (TEST_VEDGE2(frame_ptr,x,y))
                {
                    if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.ver_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.ver_edgecount[y * context.state.width + x] = 0;
            }
        }
//	printf("%6d %6d\n", hedge_count, vedge_count);
    }
    else if (context.settings.aggressive_logo_rejection==3)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if (TEST_HEDGE3(frame_ptr,x,y))
                {
                    if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.hor_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.hor_edgecount[y * context.state.width + x] = 0;
                if (TEST_VEDGE3(frame_ptr,x,y))
                {
                    if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.ver_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.ver_edgecount[y * context.state.width + x] = 0;
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection==4)
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if ((/*frame_ptr[y * width + x - edge_radius] > 50 && */ frame_ptr[y * context.state.width + x - context.settings.edge_radius] < 200) || ( /*frame_ptr[y * width + x + edge_radius] > 50 && */ frame_ptr[y * context.state.width + x + context.settings.edge_radius] < 200) )
                {
                    if (TEST_HEDGE0(frame_ptr,x,y))
                    {
                        if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.hor_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else if (frame_ptr[y * context.state.width + x] < 200)
                        context.state.hor_edgecount[y * context.state.width + x] = 0;
                }
                if ((/*frame_ptr[(y- edge_radius) * width + x ] > 50 && */ frame_ptr[(y- context.settings.edge_radius) * context.state.width + x ] < 200) || ( /*frame_ptr[(y+ edge_radius) * width + x ] > 50 && */ frame_ptr[(y+ context.settings.edge_radius) * context.state.width + x ] < 200) )
                {
                    if (TEST_VEDGE0(frame_ptr,x,y))
                    {
                        if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.ver_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else if (frame_ptr[y * context.state.width + x] < 200)
                        context.state.ver_edgecount[y * context.state.width + x] = 0;
                }
            }
        }
    }
    else
    {
        LOGO_X_LOOP
        {
            LOGO_Y_LOOP {
                if ((/*frame_ptr[y * width + x - edge_radius] > 50 && */ frame_ptr[y * context.state.width + x - context.settings.edge_radius] < 200) || ( /*frame_ptr[y * width + x + edge_radius] > 50 && */ frame_ptr[y * context.state.width + x + context.settings.edge_radius] < 200) )
                {
                    if (TEST_HEDGE0(frame_ptr,x,y))
                    {
                        if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.hor_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else
                        context.state.hor_edgecount[y * context.state.width + x] = 0;
                }
                if ((/*frame_ptr[(y- edge_radius) * width + x ] > 50 && */ frame_ptr[(y- context.settings.edge_radius) * context.state.width + x ] < 200) || ( /*frame_ptr[(y+ edge_radius) * width + x ] > 50 && */ frame_ptr[(y+ context.settings.edge_radius) * context.state.width + x ] < 200) )
                {
                    if (TEST_VEDGE0(frame_ptr,x,y))
                    {
                        if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.ver_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else
                        context.state.ver_edgecount[y * context.state.width + x] = 0;
                }
            }
        }
    }
#endif
}



double CheckStationLogoEdge(RecordingContext& context, unsigned char* testFrame)
{
    int		index;
    int		x;
    int		y;
    int		testEdges = 0;

    int goodEdges = 0;

    context.state.currentGoodEdge = 0.0;
    if (context.state.videowidth < context.state.clogoMinX || context.state.height < context.state.clogoMinY)
    {
        // No logo possible as frame size if different from where logo was found
    }
    else if (context.settings.aggressive_logo_rejection == 1)
    {
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (TEST_HEDGE1(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
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
    else if (context.settings.aggressive_logo_rejection == 2)
    {
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (TEST_HEDGE2(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
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
    else if (context.settings.aggressive_logo_rejection == 3)
    {
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (TEST_HEDGE3(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
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
    else if (context.settings.aggressive_logo_rejection == 4)
    {
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index] && testFrame[index] < 200)
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index] && testFrame[index] < 200)
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
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
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

double DoubleCheckStationLogoEdge(RecordingContext& context, unsigned char* testFrame)
{
    int		index;
    int		x;
    int		y;
    int		testEdges = 0;

    int goodEdges = 0;

    context.state.currentGoodEdge = 0.0;
    if (context.settings.aggressive_logo_rejection == 1)
    {
        for (y = context.state.tlogoMinY; y <= context.state.tlogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.tlogoMinX; x <= context.state.tlogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (TEST_HEDGE1(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
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
    else if (context.settings.aggressive_logo_rejection == 2)
    {
        for (y = context.state.tlogoMinY; y <= context.state.tlogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.tlogoMinX; x <= context.state.tlogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (TEST_HEDGE2(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
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
    else if (context.settings.aggressive_logo_rejection == 3)
    {
        for (y = context.state.tlogoMinY; y <= context.state.tlogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.tlogoMinX; x <= context.state.tlogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (TEST_HEDGE3(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
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
    else if (context.settings.aggressive_logo_rejection == 4)
    {
        for (y = context.state.tlogoMinY; y <= context.state.tlogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.tlogoMinX; x <= context.state.tlogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index] && testFrame[index] < 200)
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index] && testFrame[index] < 200)
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
        for (y = context.state.tlogoMinY; y <= context.state.tlogoMaxY; y += context.settings.edge_step)
        {
            for (x = context.state.tlogoMinX; x <= context.state.tlogoMaxX; x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (TEST_HEDGE0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
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

void InitProcessLogoTest(RecordingContext& context)
{
    context.state.logo_block_count = 0;
    context.state.logoTrendCounter = 0;
    context.state.frames_with_logo = 0;
    context.state.lastLogoTest = false;
    context.state.curLogoTest = false;
}


#define LOGO_SAMPLE (int)(context.settings.fps * context.state.logoFreq)

bool ProcessLogoTest(RecordingContext& context, int framenum_real, int curLogoTest, int close)
{



    int i;
    double s1,s2;

    if (context.settings.logo_filter > 0)
    {
        if (!close)
        {

            if (framenum_real > context.settings.logo_filter * 2 * LOGO_SAMPLE)
            {
                s1 = s2 = 0.0;
                for ( i = 0; i < context.settings.logo_filter; i++)
                {
                    s1 += (context.state.frame[framenum_real - i * LOGO_SAMPLE - context.settings.logo_filter * LOGO_SAMPLE].currentGoodEdge - context.settings.logo_threshold > 0 ? 1 : -1);
                    s2 += (context.state.frame[framenum_real - i * LOGO_SAMPLE].currentGoodEdge - context.settings.logo_threshold > 0 ? 1 : -1);
                }
                s1 /= context.settings.logo_filter;
                s2 /= context.settings.logo_filter;
                for (i = 0; i < LOGO_SAMPLE; i++)
                {
                    context.state.frame[framenum_real - context.settings.logo_filter * LOGO_SAMPLE - i].logo_filter = (s1 + s2);
                }
            }
            for (i = 0; i < LOGO_SAMPLE; i++)
            {
                context.state.frame[framenum_real - i].logo_filter = 0.0;
            }

            framenum_real -= context.settings.logo_filter*LOGO_SAMPLE;
            if (framenum_real < 0) framenum_real= 1;

            curLogoTest = (context.state.frame[framenum_real].logo_filter > 0.0 ? 1 : 0);
        }
        else
            curLogoTest = false;
    }

    if (curLogoTest != context.state.lastLogoTest)
    {
        if (!curLogoTest)
        {
            // Logo disappeared
            context.state.lastLogoTest = false;
            context.state.logoTrendCounter = 0;
            context.state.logo_block[context.state.logo_block_count].end = framenum_real - 1 * (int)(context.settings.fps * context.state.logoFreq);
            if (context.state.logo_block[context.state.logo_block_count].end - context.state.logo_block[context.state.logo_block_count].start >
                    2*(int)(context.settings.shrink_logo*context.settings.fps) + (context.settings.shrink_logo_tail*context.settings.fps) )
            {
                context.state.logo_block[context.state.logo_block_count].end -= (int)(context.settings.shrink_logo*context.settings.fps) + (int)(context.settings.shrink_logo_tail*context.settings.fps);
                context.state.logo_block[context.state.logo_block_count].start += (int)(context.settings.shrink_logo*context.settings.fps);
                context.state.frames_with_logo -= 2 * (int)(context.settings.fps * context.state.logoFreq) + 2*(int)(context.settings.shrink_logo*context.settings.fps) + (int)(context.settings.shrink_logo_tail*context.settings.fps);
                if (context.state.framearray)
                {
                    i = context.state.logo_block[context.state.logo_block_count].end;
                    if (i<0) i = 0;
                    for (; i < framenum_real; i++)
                        context.state.frame[i].logo_present = false;
                }
                Debug
                (context, 3,
                 "\nEnd logo block %i\tframe %i\tLength - %s\n",
                 context.state.logo_block_count,
                 context.state.logo_block[context.state.logo_block_count].end,
                 dblSecondsToStrMinutes(context, F2L(context.state.logo_block[context.state.logo_block_count].end, context.state.logo_block[context.state.logo_block_count].start))
                );
                context.state.logo_block_count++;
                InitializeLogoBlockArray(context,  context.state.logo_block_count);
            }
            else
            {
                context.state.logo_block[context.state.logo_block_count].start = -1; // else discard logo cblock
            }
        }
        else
        {
            // real change or false change?
            context.state.logoTrendCounter++;
            if (context.state.logoTrendCounter == context.state.minHitsForTrend)
            {
                context.state.lastLogoTest = true;
                context.state.logoTrendCounter = 0;
                InitializeLogoBlockArray(context, context.state.logo_block_count + 2);
                context.state.logo_block[context.state.logo_block_count + 1].start = -1;
                context.state.logo_block[context.state.logo_block_count].start = max(framenum_real - ((int)(context.settings.fps * context.state.logoFreq) * (context.state.minHitsForTrend - 1)),0);
                context.state.frames_with_logo +=((int)(context.settings.fps * context.state.logoFreq) * (context.state.minHitsForTrend - 1));
                if (context.state.framearray)
                {
                    for (i = context.state.logo_block[context.state.logo_block_count].start; i < framenum_real; i++)
                        context.state.frame[i].logo_present = true;
                }
                if (!context.state.logo_block_count)
                {
                    Debug(context,
                        3,
                        "\t\t\t\tStart logo cblock %i\tframe %i\n",
                        context.state.logo_block_count,
                        context.state.logo_block[context.state.logo_block_count].start
                    );
                }
                else
                {
                    Debug(context,
                        3,
                        "\n\t\t\t\tNonlogo Length - %s\nStart logo cblock %i\tframe %i\n",
                        dblSecondsToStrMinutes(context, F2L(context.state.logo_block[context.state.logo_block_count].start, context.state.logo_block[context.state.logo_block_count - 1].end)),
                        context.state.logo_block_count,
                        context.state.logo_block[context.state.logo_block_count].start
                    );
                }

            }
        }
    }
    else
    {
        context.state.logoTrendCounter = 0;
    }


    return(context.state.lastLogoTest);
}


void ResetLogoBuffers(RecordingContext& context)
{
    context.state.newestLogoBuffer = context.state.oldestLogoBuffer = 0;
    if (!context.state.logoFrameNum.empty()) {
        if (context.state.newestLogoBuffer == context.settings.num_logo_buffers) context.state.newestLogoBuffer = 0; // rotates buffer
        context.state.logoFrameNum[context.state.newestLogoBuffer] = context.state.framenum_real;
        context.state.oldestLogoBuffer = 0;
    /*
         for (i = 0; i < num_logo_buffers; i++) {
              free(logoFrameBuffer[i]);
         }
         for (i = 0; i < num_logo_buffers; i++) {
              logoFrameBuffer[i] = malloc(width * height * sizeof(frame_ptr[0]));
              if (logoFrameBuffer[i] == NULL) {
                   Debug(0, "Could not allocate memory for logo frame buffer %i\n", i);
                   comskip::request_exit(16);
              }
         */
    }
}

void FillLogoBuffer(RecordingContext& context)
{
    int i;
    context.state.newestLogoBuffer++;
    if (context.state.newestLogoBuffer == context.settings.num_logo_buffers) context.state.newestLogoBuffer = 0; // rotates buffer
    context.state.logoFrameNum[context.state.newestLogoBuffer] = context.state.framenum_real;
    context.state.oldestLogoBuffer = 0;
    for (i = 0; i < context.settings.num_logo_buffers; i++)
    {
        if (context.state.logoFrameNum[i]  && context.state.logoFrameNum[i] < context.state.logoFrameNum[context.state.oldestLogoBuffer]) context.state.oldestLogoBuffer = i;
    }

    i = min((unsigned int)context.state.logoFrameBufferSize, context.state.width * context.state.height * sizeof(context.state.frame_ptr[0]));
    memcpy(context.state.logoFrameBuffer[context.state.newestLogoBuffer].data(), context.state.frame_ptr, i);

//	for (y = 0; y < height; y++) {
//		for (x = 0; x < width; x++) {
//			logoFrameBuffer[newestLogoBuffer][y * width + x] = frame_ptr[y * width + x];
//		}
//	}

    EdgeDetect(context, context.state.logoFrameBuffer[context.state.newestLogoBuffer].data(), context.state.newestLogoBuffer);
    if ((!context.state.logoBuffersFull) && (context.state.newestLogoBuffer == context.settings.num_logo_buffers - 1)) context.state.logoBuffersFull = true;
}

bool SearchForLogoEdges(RecordingContext& context)
{
    int		i;
    int		x;
    int		y;
    double scale = ((double)context.state.height / 572) * ( (double) context.state.videowidth / 720 );
    double	logoPercentageOfScreen;
    bool	LogoIsThere;
    int		sum;
    int		tempMinX;
    int		tempMaxX;
    int		tempMinY;
    int		tempMaxY;
    int		last_non_logo_frame;
    int		logoFound = false;
    context.state.tlogoMinX = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxX = context.state.videowidth - context.settings.edge_radius - context.settings.border;
    context.state.tlogoMinY = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxY = context.state.height - context.settings.edge_radius - context.settings.border;
#if MULTI_EDGE_BUFFER
    memset(thoriz_edgemask, 1, context.state.width * context.state.height);
    memset(ttvert_edgemask, 1, context.state.width * context.state.height);
    for (i = 0; i < 1; i++)
    {
        for (y = border; y < context.state.height - border; y++)
        {
            for (x = border; x < context.state.videowidth - border; x++)
            {
                if (!thoriz_edgemask[y * context.state.width + x] || !horiz_edges[i][y * context.state.width + x])
                {
                    thoriz_edgemask[y * context.state.width + x] = 0;
                }

                if (!tvert_edgemask[y * context.state.width + x] || !vert_edges[i][y * context.state.width + x])
                {
                    tvert_edgemask[y * context.state.width + x] = 0;
                }
            }
        }
    }
#if 0
    for (y = border; y < context.state.height - border; y++)
    {
        for (x = border; x < context.state.videowidth - border; x++)
        {
            index = y * context.state.width + x;
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
        for (y = border; y < context.state.height - border; y++)
        {
            for (x = border; x < context.state.videowidth - border; x++)
            {
                if (!thoriz_edgemask[y * context.state.width + x] || !horiz_edges[i][y * context.state.width + x])
                {
                    thoriz_edgemask[y * context.state.width + x] = 0;
                }

                if (!tvert_edgemask[y * context.state.width + x] || !vert_edges[i][y * context.state.width + x])
                {
                    tvert_edgemask[y * context.state.width + x] = 0;
                }
            }
        }
    }
#endif
#else
    memset(context.state.thoriz_edgemask.data(), 0, context.state.width * context.state.height);
    memset(context.state.tvert_edgemask.data(), 0, context.state.width * context.state.height);
//	minY = (logo_at_bottom ? height/2 : edge_radius + (int)(height * borderIgnore));
//	if (framearray) minY = max(minY, frame[frame_count].minY);
//	maxY = (subtitles? height/2 : height - edge_radius - (int)(height * borderIgnore));
//	if (framearray) maxY = min(maxY, frame[frame_count].maxY);

    LOGO_X_LOOP
    {
        LOGO_Y_LOOP {
//	for (y = minY; y < maxY; y++) {
//		for (x = edge_radius + (int)(width * borderIgnore); x < videowidth - edge_radius + (int)(width * borderIgnore); x++) {
            if (context.state.hor_edgecount[y * context.state.width + x] >= context.settings.num_logo_buffers * 0.95 )
            {
                context.state.thoriz_edgemask[y * context.state.width + x] = 1;
            }
            if (context.state.ver_edgecount[y * context.state.width + x] >= context.settings.num_logo_buffers * 0.95 )
            {
                context.state.tvert_edgemask[y * context.state.width + x] = 1;
            }
        }
    }
#endif

    ClearEdgeMaskArea(context, context.state.thoriz_edgemask.data(), context.state.tvert_edgemask.data());
    ClearEdgeMaskArea(context, context.state.tvert_edgemask.data(), context.state.thoriz_edgemask.data());


    SetEdgeMaskArea(context, context.state.thoriz_edgemask.data());
    tempMinX = context.state.tlogoMinX;
    tempMaxX = context.state.tlogoMaxX;
    tempMinY = context.state.tlogoMinY;
    tempMaxY = context.state.tlogoMaxY;
    context.state.tlogoMinX = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxX = context.state.videowidth - context.settings.edge_radius - context.settings.border;
    context.state.tlogoMinY = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxY = context.state.height - context.settings.edge_radius - context.settings.border;
    SetEdgeMaskArea(context, context.state.tvert_edgemask.data());
    if (tempMinX < context.state.tlogoMinX) context.state.tlogoMinX = tempMinX;
    if (tempMaxX > context.state.tlogoMaxX) context.state.tlogoMaxX = tempMaxX;
    if (tempMinY < context.state.tlogoMinY) context.state.tlogoMinY = tempMinY;
    if (tempMaxY > context.state.tlogoMaxY) context.state.tlogoMaxY = tempMaxY;
    context.state.edgemask_filled = 1;
    logoPercentageOfScreen = (double)((context.state.tlogoMaxY - context.state.tlogoMinY) * (context.state.tlogoMaxX - context.state.tlogoMinX)) / (double)(context.state.height * context.state.width);
    if (logoPercentageOfScreen > context.settings.logo_max_percentage_of_screen)
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

    i = CountEdgePixels(context);
//printf("Edges=%d\n",i);
//	if (i > 350/(lowres+1)/(edge_step)) {
    if ( i > 150 * scale /context.settings.edge_step)
    {
        logoPercentageOfScreen = (double)((context.state.tlogoMaxY - context.state.tlogoMinY) * (context.state.tlogoMaxX - context.state.tlogoMinX)) / (double)(context.state.height * context.state.width);
        if (i > 40000 || logoPercentageOfScreen > context.settings.logo_max_percentage_of_screen)
        {
            Debug(context,
                3,
                "Edge count - %i\tPercentage of screen - %.2f%% TOO BIG, CAN'T BE A LOGO.\n",
                i,
                logoPercentageOfScreen * 100
            );
//			logoInfoAvailable = false;
        }
        else
        {
            Debug(context, 3, "Edge count - %i\tPercentage of screen - %.2f%%, Check: %i\n", i, logoPercentageOfScreen * 100,context.state.doublCheckLogoCount);
//			logoInfoAvailable = true;
            logoFound = true;
        }
    }
    else
        Debug(context, 3, "Not enough edge count - %i\n", i);


    if (logoFound)
    {
        context.state.doublCheckLogoCount++;
        Debug(context, 3, "Double checking - %i\n", context.state.doublCheckLogoCount );

        if (context.state.doublCheckLogoCount > 1)
        {
            // Final check done, found
        }
        else
            logoFound = false;
    }
    else
    {
        context.state.doublCheckLogoCount = 0;
    }


    sum = 0;
    context.state.oldestLogoBuffer = 0;
    for (i = 0; i < context.settings.num_logo_buffers; i++)
    {
        if (context.state.logoFrameNum[i]  && context.state.logoFrameNum[i] < context.state.logoFrameNum[context.state.oldestLogoBuffer]) context.state.oldestLogoBuffer = i;
    }
    last_non_logo_frame = context.state.logoFrameNum[context.state.oldestLogoBuffer];
    if (logoFound)
    {
        Debug(context, 3, "Doublechecking frames %i to %i for logo.\n", context.state.logoFrameNum[context.state.oldestLogoBuffer], context.state.logoFrameNum[context.state.newestLogoBuffer]);
        for (i = 0; i < context.settings.num_logo_buffers; i++)
        {
            context.state.currentGoodEdge = DoubleCheckStationLogoEdge(context, context.state.logoFrameBuffer[i].data());
            LogoIsThere = (context.state.currentGoodEdge > context.settings.logo_threshold);

            for (x = context.state.logoFrameNum[i]; x < context.state.logoFrameNum[i] + (int)( context.state.logoFreq * context.settings.fps ); x++)
            {
                context.state.frame[x].currentGoodEdge = context.state.currentGoodEdge;
                context.state.frame[x].logo_present = LogoIsThere;
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
                Debug(context, 7, "Logo not present in frame %i.\n", context.state.logoFrameNum[i]);
            }
        }
    }


    if (logoFound && (sum >= (int)(context.settings.num_logo_buffers * .9)))
    {

        context.state.clogoMinX = context.state.tlogoMinX;
        context.state.clogoMaxX = context.state.tlogoMaxX;
        context.state.clogoMinY = context.state.tlogoMinY;
        context.state.clogoMaxY = context.state.tlogoMaxY;
        memcpy(context.state.choriz_edgemask.data(), context.state.thoriz_edgemask.data(), context.state.width * context.state.height);
        memcpy(context.state.cvert_edgemask.data(), context.state.tvert_edgemask.data(), context.state.width * context.state.height);


        context.state.logoTrendCounter = context.settings.num_logo_buffers;
        context.state.lastLogoTest = true;
        context.state.curLogoTest = true;

        context.state.logo_block[context.state.logo_block_count].start = last_non_logo_frame+1;
        DumpEdgeMasks(context);
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
        InitScanLines(context);
        InitHasLogo(context);

        context.state.logoInfoAvailable = true; //xxxxxxx
    }
    else
    {
//		logoInfoAvailable = false; //xxxxxxx
        context.state.currentGoodEdge = 0.0;
    }

    if (!context.state.logoInfoAvailable && context.settings.startOverAfterLogoInfoAvail && (context.state.framenum_real > (int)(context.settings.giveUpOnLogoSearch * context.settings.fps)))
    {
        Debug(context, 1, "%s", context.translator.format("detection_no_logo", context.state.framenum_real).c_str());
        context.settings.commDetectMethod -= LOGO;
    }
    if (context.settings.added_recording > 0)
        context.settings.giveUpOnLogoSearch += context.settings.added_recording * 60;

    if (context.state.logoInfoAvailable && context.settings.startOverAfterLogoInfoAvail)
    {
        Debug(context, 3, "Logo found at frame %i\tlogoMinX=%i\tlogoMaxX=%i\tlogoMinY=%i\tlogoMaxY=%i\n", context.state.framenum_real, context.state.clogoMinX, context.state.clogoMaxX, context.state.clogoMinY, context.state.clogoMaxY);
        SaveLogoMaskData(context);
        Debug(context, 3, "******************* End of Logo Processing ***************\n");
        return false;
    }

    return true;
}


#define MAX_SEARCH_FRACTION 0.02

int ClearEdgeMaskArea(RecordingContext& context, unsigned char* temp, unsigned char* test)
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
            if (temp[y * context.state.width + x] == 1)
            {
                if (test[y * context.state.width + x] == 1)
//					goto found;
                    count++;

                for (offset = context.settings.edge_step; offset < (int) (MAX_SEARCH_FRACTION * context.state.width); offset += context.settings.edge_step)
                {
                    iy = min(y+offset,context.state.height-1);
                    for (ix= max(x-offset,0); ix <= min(x+offset, context.state.width-1); ix += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;

                    iy = max(y-offset,0);
                    for (ix= max(x-offset,0); ix <= min(x+offset, context.state.width-1); ix += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;

                    ix = min(x+offset, context.state.width-1);
                    for (iy= max(y-offset+context.settings.edge_step,0); iy <=  min(y+offset-context.settings.edge_step,context.state.height-1); iy += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;

                    ix = max(x-offset,0);
                    for (iy= max(y-offset+context.settings.edge_step,0); iy <=  min(y+offset-context.settings.edge_step,context.state.height-1); iy += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;
                    if (count >= context.settings.edge_weight)
                        goto found;
                }
                temp[y * context.state.width + x] = 0;
                continue;
found:
                valid++;
            }
        }
    }
    return(valid);
}

void SetEdgeMaskArea(RecordingContext& context, unsigned char* temp)
{
    int x;
    int y;
    context.state.tlogoMinX = context.state.videowidth - 1;
    context.state.tlogoMaxX = 0;
    context.state.tlogoMinY = context.state.height - 1;
    context.state.tlogoMaxY = 0;
    LOGO_X_LOOP
//    for (y = (logo_at_bottom ? height/2 : border + edge_radius); y < (subtitles? height/2 : height - border - edge_radius); y++)
    {
        LOGO_Y_LOOP
//        for (x = border+edge_radius; x < videowidth - border - edge_radius; x++)
        {
            if (temp[y * context.state.width + x] == 1)
            {
                if (x - LOGOBORDER < context.state.tlogoMinX) context.state.tlogoMinX = x - LOGOBORDER;
                if (y - LOGOBORDER < context.state.tlogoMinY) context.state.tlogoMinY = y - LOGOBORDER;
                if (x + LOGOBORDER > context.state.tlogoMaxX) context.state.tlogoMaxX = x + LOGOBORDER;
                if (y + LOGOBORDER > context.state.tlogoMaxY) context.state.tlogoMaxY = y + LOGOBORDER;
            }
        }
    }

    if (context.state.tlogoMinX < context.settings.edge_radius) context.state.tlogoMinX = context.settings.edge_radius;
    if (context.state.tlogoMaxX > (context.state.videowidth - context.settings.edge_radius)) context.state.tlogoMaxX = (context.state.videowidth - context.settings.edge_radius);
    if (context.state.tlogoMinY < context.settings.edge_radius) context.state.tlogoMinY = context.settings.edge_radius;
    if (context.state.tlogoMaxY > (context.state.height - context.settings.edge_radius)) context.state.tlogoMaxY = (context.state.height - context.settings.edge_radius);
}

int CountEdgePixels(RecordingContext& context)
{
    int x;
    int y;
    int count = 0;
    int hcount = 0;
    int vcount = 0;
    for (y = context.state.tlogoMinY; y <= context.state.tlogoMaxY; y++)
    {
        for (x = context.state.tlogoMinX; x <= context.state.tlogoMaxX; x++)
        {
            if (context.state.thoriz_edgemask[y * context.state.width + x]) hcount++;
            if (context.state.tvert_edgemask[y * context.state.width + x]) vcount++;
        }
    }
    count = hcount + vcount;
//    if (count>0)
//        Debug(1, "\nFrame[%d] edgecount=%d",framenum_real, count);
//	printf("%6d %6d\n",hcount, vcount);
    //if ((hcount < 50 * scale / edge_step) || (vcount < 50 * scale /edge_step )) count = 0;
    return (count);
}

void DumpEdgeMask(RecordingContext& context, unsigned char* buffer, int direction)
{
    int x;
    int y;
    char outbuf[MAXWIDTH+1];
    switch (direction)
    {
    case HORIZ:
        Debug(context, 1, "\nHorizontal Logo Mask \n     ");
        break;

    case VERT:
        Debug(context, 1, "\nVertical Logo Mask \n     ");
        break;

    case DIAG1:
        Debug(context, 1, "\nDiagonal 1 Logo Mask \n     ");
        break;

    case DIAG2:
        Debug(context, 1, "\nDiagonal 2 Logo Mask \n     ");
        break;
    }

    for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
    {
        outbuf[x-context.state.clogoMinX] = '0'+ (x % 10);
    }
    outbuf[x-context.state.clogoMinX] = 0;
    Debug(context, 1, "%s\n",outbuf);


    Debug(context, 1, "\n");
    for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
    {
        Debug(context, 1, "%3d: ", y);
        for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
        {
            switch (buffer[y * context.state.width + x])
            {
            case 0:
                outbuf[x-context.state.clogoMinX] = ' ';
                break;

            case 1:
                outbuf[x-context.state.clogoMinX] = '*';
                break;
            }
        }
        outbuf[x-context.state.clogoMinX] = 0;
        Debug(context, 1, "%s\n",outbuf);

    }
}

void DumpEdgeMasks(RecordingContext& context)
{
    int x;
    int y;
    char outbuf[MAXWIDTH+1];

    for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
    {
        outbuf[x-context.state.clogoMinX] = '0'+ (x % 10);
    }
    outbuf[x-context.state.clogoMinX] = 0;
    Debug(context, 1, "%s\n",outbuf);

    for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
    {
        Debug(context, 1, "%3d: ", y);
        for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
        {
            switch (context.state.choriz_edgemask[y * context.state.width + x])
            {
            case 0:
                if (context.state.cvert_edgemask[y * context.state.width + x] == 1)
                    outbuf[x-context.state.clogoMinX] =  '-';
                else
                    outbuf[x-context.state.clogoMinX] =  ' ';
                break;

            case 1:
                if (context.state.cvert_edgemask[y * context.state.width + x] == 1)
                    outbuf[x-context.state.clogoMinX] =  '+';
                else
                    outbuf[x-context.state.clogoMinX] =  '|';
                break;
            }
        }
        outbuf[x-context.state.clogoMinX] = 0;
        Debug(context, 1, "%s\n",outbuf);
    }
}

bool CheckFramesForLogo(RecordingContext& context, int start, int end)
{
    int		i;
#ifdef OLD_LIVE_TV
    int		j;
    for (i = start; i <= end; i++)
    {
        for (j = 0; j < context.state.logo_block_count; j++)
        {
            if (i > context.state.logo_block[j].start && i < context.state.logo_block[j].end)
            {
                return (!reverseLogoLogic);
            }
        }
    }

    return (reverseLogoLogic);
#else
    double sum = 0.0;
    for (i = start; i <= end; i++)
        sum += (context.state.frame[i].currentGoodEdge > context.settings.logo_threshold ? 1 : 0);

    sum = sum / (end - start + 1);
    if (sum > context.settings.logo_percentage_threshold)
        return(true);
    return(false);

#endif

}

double CalculateLogoFraction(RecordingContext& context, int start, int end)
{
    int		i,j;
    int		count=0;
    j = 0;
    for (i = start; i <= end; i++)
    {
        while (j < context.state.logo_block_count && i > context.state.logo_block[j].end) j++;
        if (j < context.state.logo_block_count && i >= context.state.logo_block[j].start && i <= context.state.logo_block[j].end )
            count++;
    }
    if (context.state.reverseLogoLogic)
        return (1.0 - (double) count / (double)(end - start + 1));
    return ((double) count / (double)(end - start + 1));
}

bool CheckFrameForLogo(RecordingContext& context, int i)
{
    int		j=0;
    while (j < context.state.logo_block_count && i > context.state.logo_block[j].end) j++;
    if (j < context.state.logo_block_count && i <= context.state.logo_block[j].end && i >= context.state.logo_block[j].start )
    {
        return(!context.state.reverseLogoLogic);
    }
    return (context.state.reverseLogoLogic);
}



char CheckFramesForCommercial(RecordingContext& context, int start, int end)
{
    int		i;
    if (start >= end )
        return ('0');						// Too short to decide
    i = 0;
    while (i <= context.state.commercial_count && start > context.state.commercial[i].end_frame)
        i++;
    if (i <= context.state.commercial_count)  			// Now start <= commercial[i].end_frame
    {
        if (end < context.state.commercial[i].start_frame)
            return('+');
        if (start < context.state.commercial[i].start_frame)
            return('0');
        return('-');
    }
    return('+');
}

char CheckFramesForReffer(RecordingContext& context, int start, int end)
{
    int		i;
    if (context.state.reffer_count < 0)
        return(' ');
    if (start >= end )
        return ('0');						// Too short to decide
    i = 0;
    while (i <= context.state.reffer_count &&  context.state.reffer[i].end_frame < start + context.settings.fps)
        i++;
    if (i <= context.state.reffer_count)  			// Now start <= reffer[i].end_frame
    {
        if (context.state.reffer[i].start_frame < start + context.settings.fps)
            return('-');
        if (context.state.reffer[i].start_frame > end - context.settings.fps)
            return('+');
        if ( context.state.reffer[i].start_frame < end + context.settings.fps)
            return('0');
        return('-');
    }
    return('+');
}

void SaveLogoMaskData(RecordingContext& context)
{
    FILE*	logo_file;
    int		x;
    int		y;
    logo_file = myfopen(context.state.logofilename.c_str(), "w");
    if (!logo_file)
    {
        const auto message = context.translator.format("create_failed", strerror(errno), context.state.logofilename);
        fputs(message.c_str(), stderr);
        Debug(context, 1, "%s", message.c_str());
        if(context.settings.startOverAfterLogoInfoAvail)
            comskip::request_exit(7);
        return;
    }

    fprintf(logo_file, "logoMinX=%i\n", context.state.clogoMinX);
    fprintf(logo_file, "logoMaxX=%i\n", context.state.clogoMaxX);
    fprintf(logo_file, "logoMinY=%i\n", context.state.clogoMinY);
    fprintf(logo_file, "logoMaxY=%i\n", context.state.clogoMaxY);
    fprintf(logo_file, "picWidth=%i\n", context.state.width);
    fprintf(logo_file, "picHeight=%i\n", context.state.height);
    if (1)
    {
        fprintf(logo_file, "\nCombined Logo Mask\n");
        fprintf(logo_file, "\202\n");
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
            {
                switch (context.state.choriz_edgemask[y * context.state.width + x])
                {
                case 0:
                    if (context.state.cvert_edgemask[y * context.state.width + x] == 1)
                        fprintf(logo_file, "-");
                    else
                        fprintf(logo_file, " ");
                    break;

                case 1:
                    if (context.state.cvert_edgemask[y * context.state.width + x] == 1)
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
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
            {
                switch (context.state.choriz_edgemask[y * context.state.width + x])
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
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
            {
                switch (context.state.cvert_edgemask[y * context.state.width + x])
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

void LoadLogoMaskData(RecordingContext& context)
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

    logo_file = myfopen(context.state.logofilename.c_str(), "r");
    if (logo_file)
    {
        Debug(context, 1, "Using %s for logo data.\n", context.state.logofilename.c_str());
        len = fread(data, 1, 1999, logo_file);
        fclose(logo_file);
        data[len] = '\0';
        if ((tmp = FindNumber(context, data, "picWidth=", (double) context.state.width)) > -1) context.state.videowidth = context.state.width = (int)tmp;
        if ((tmp = FindNumber(context, data, "picHeight=", (double) context.state.height)) > -1) context.state.height = (int)tmp;
        context.state.ensure_pixel_buffers(true);
        if ((tmp = FindNumber(context, data, "logoMinX=", (double) context.state.clogoMinX)) > -1) context.state.clogoMinX = (int)tmp;
        if ((tmp = FindNumber(context, data, "logoMaxX=", (double) context.state.clogoMaxX)) > -1) context.state.clogoMaxX = (int)tmp;
        if ((tmp = FindNumber(context, data, "logoMinY=", (double) context.state.clogoMinY)) > -1) context.state.clogoMinY = (int)tmp;
        if ((tmp = FindNumber(context, data, "logoMaxY=", (double) context.state.clogoMaxY)) > -1) context.state.clogoMaxY = (int)tmp;
        comskip::detection::validate_logo_bounds(context.state.width, context.state.height,
            context.state.clogoMinX, context.state.clogoMaxX, context.state.clogoMinY, context.state.clogoMaxY);
    }
    else
    {
        Debug(context, 0, "%s", context.translator.text("detection_logo_file_missing"));
        context.state.logoInfoAvailable = false;
        return;
    }

    context.state.ensure_pixel_buffers(true);
    logo_file = myfopen(context.state.logofilename.c_str(), "r");
    /*
        choriz_edgemask = malloc(width * height * sizeof(unsigned char));
        if (choriz_edgemask == NULL) {
            Debug(0, "Could not allocate memory for horizontal edgemask\n");
            comskip::request_exit(8);
        }

        cvert_edgemask = malloc(width * height * sizeof(unsigned char));
        if (cvert_edgemask == NULL) {
            Debug(0, "Could not allocate memory for vertical edgemask\n");
            comskip::request_exit(9);
        }
        memset(choriz_edgemask, 0, width * height);
        memset(cvert_edgemask, 0, width * height);
    */
    do
    {
        temp = getc(logo_file);
    }
    while ((temp != '\200') && !feof(logo_file));
    for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
    {
        for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
        {
            temp = getc(logo_file);
            if (temp == '\n') temp = getc(logo_file);				// If a carrage return was retrieved, get the next character
            switch (temp)
            {
            case ' ':
                context.state.choriz_edgemask[y * context.state.width + x] = 0;
                break;

            case '|':
                context.state.choriz_edgemask[y * context.state.width + x] = 1;
                break;
            }
        }
    }

    fclose(logo_file);
    logo_file = myfopen(context.state.logofilename.c_str(), "r");
    do
    {
        temp = getc(logo_file);
    }
    while ((temp != '\201') && !feof(logo_file));
    for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
    {
        for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
        {
            temp = getc(logo_file);
            if (temp == '\n') temp = getc(logo_file);				// If a carrage return was retrieved, get the next character
            switch (temp)
            {
            case ' ':
                context.state.cvert_edgemask[y * context.state.width + x] = 0;
                break;

            case '-':
                context.state.cvert_edgemask[y * context.state.width + x] = 1;
                break;
            }
        }
    }
    fclose(logo_file);

    logo_file = myfopen(context.state.logofilename.c_str(), "r");
    do
    {
        temp = getc(logo_file);
    }
    while ((temp != '\202') && !feof(logo_file));
    if (!feof(logo_file))
    {
        for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
        {
            for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
            {
                temp = getc(logo_file);
                if (temp == '\n') temp = getc(logo_file);				// If a carrage return was retrieved, get the next character
                switch (temp)
                {
                case ' ':
                    context.state.choriz_edgemask[y * context.state.width + x] = 0;
                    context.state.cvert_edgemask[y * context.state.width + x] = 0;
                    break;

                case '-':
                    context.state.choriz_edgemask[y * context.state.width + x] = 0;
                    context.state.cvert_edgemask[y * context.state.width + x] = 1;
                    break;

                case '|':
                    context.state.choriz_edgemask[y * context.state.width + x] = 1;
                    context.state.cvert_edgemask[y * context.state.width + x] = 0;
                    break;

                case '+':
                    context.state.choriz_edgemask[y * context.state.width + x] = 1;
                    context.state.cvert_edgemask[y * context.state.width + x] = 1;
                    break;

                }
            }
        }
    }
    fclose(logo_file);


    context.state.logoInfoAvailable = true;
    context.settings.startOverAfterLogoInfoAvail = true; // prevent continuous searching for logo when a logo file is specified
    context.state.secondLogoSearch = true;
    InitScanLines(context);
    InitHasLogo(context);
    context.state.isSecondPass = true;
    if (!context.state.loadingCSV)
    {
//		DumpEdgeMask(choriz_edgemask, HORIZ);
//		DumpEdgeMask(cvert_edgemask, VERT);
        DumpEdgeMasks(context);
    }
    memset(data, 0, sizeof(data));
    _flushall();
    if (context.settings.output_default)
    {
        txt_file = myfopen(context.state.out_filename.c_str(), "r");
        if (!txt_file)
        {
            sleep_for_ms(50L);
            txt_file = myfopen(context.state.out_filename.c_str(), "r");
            if (!txt_file)
            {
                Debug(context, 0, "%s", context.translator.format("detection_output_read_failed", context.state.out_filename.c_str()).c_str());
                context.state.isSecondPass = false;
                return;
            }
        }


        if(fseek( txt_file, 0L, SEEK_SET ))
        {
            Debug(context, 0, "%s", context.translator.text("detection_output_seek_failed"));
        }


        while (fgets(data, 1999, txt_file) != NULL)
        {
            if (strstr(data, "FILE PROCESSING COMPLETE") != NULL)
            {
                context.state.lastFrame = 0;
                break;
            }
            ptr = strchr(data, '\t');
            if (ptr != NULL)
            {
                ptr++;
                tmpLong = strtol(ptr, NULL, 10);
                if (tmpLong > context.state.lastFrame)
                {
                    context.state.lastFrame = tmpLong;
                }
            }
        }
        fclose(txt_file);
    }
    Debug(context, 10, "The last frame found in %s was %i\n", context.state.out_filename.c_str(), context.state.lastFrame);
}
