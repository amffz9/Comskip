#include "exit_requested.h"
#include "legacy_detection.h"

void FindIniFile(RecordingContext& context)
{
#ifdef _WIN32
    char	searchinifile[] = "comskip.ini";
    char	searchexefile[] = "comskip.exe";
    char	searchdictfile[] = "comskip.dictionary";
    char	envvar[] = "PATH";
    _searchenv(searchinifile, envvar, context.state.inifilename);
    if (*context.state.inifilename != '\0')
    {
        Debug(context, 1, "Path for %s: %s\n", searchinifile, context.state.inifilename);
    }
    else
    {
        Debug(context, 1, "%s not found\n", searchinifile);
    }

    _searchenv(searchdictfile, envvar, context.state.dictfilename);
    if (*context.state.dictfilename != '\0')
    {
        Debug(context, 1, "Path for %s: %s\n", searchdictfile, context.state.dictfilename);
    }
    else
    {
        Debug(context, 1, "%s not found\n", searchdictfile);
    }

    _searchenv(searchexefile, envvar, context.state.exefilename);
    if (*context.state.exefilename != '\0')
    {
        Debug(context, 1, "Path for %s: %s\n", searchexefile, context.state.exefilename);
    }
    else
    {
        Debug(context, 1, "%s not found\n", searchexefile);
    }
#endif
}

double FindScoreThreshold(RecordingContext& context, double percentile)
{
    int			i;
    int			counter;
    double*		score = NULL;
    long*		count = NULL;
    long*		start = NULL;
    double*		percent = NULL;
    int*		blocknr = NULL;
    double		tempScore;
    long		tempCount;
    long		tempStart;
    int			tempBlocknr;
    long		targetCount;
    long		totalframes = 0;
    bool		hadToSwap = false;
    score = static_cast<double *>( malloc(sizeof(context.state.cblock[0].score) * context.state.block_count) );
    count = static_cast<long *>( malloc(sizeof(long) * context.state.block_count) );
    start = static_cast<long *>( malloc(sizeof(long) * context.state.block_count) );
    blocknr = static_cast<int *>( malloc(sizeof(int) * context.state.block_count) );
    percent = static_cast<double *>( malloc(sizeof(double) * context.state.block_count) );
    if ((score == NULL) || (count == NULL) || (start == NULL) || (blocknr == NULL) || (percent == NULL))
    {
        Debug(context, 1, "Could not allocate memory.  Exiting program.\n");
        comskip::request_exit(21);
    }

    counter = 0;
    for (i = 0; i < context.state.block_count; i++)
    {
        blocknr[i] = i;
        score[i] = context.state.cblock[i].score;
        count[i] = context.state.cblock[i].f_end - context.state.cblock[i].f_start + 1;
        start[i] = context.state.cblock[i].f_start;
    }

    do
    {
        hadToSwap = false;
        counter++;
        for (i = 0; i < context.state.block_count - 1; i++)
        {
            if (score[i] > score[i + 1])
            {
                hadToSwap = true;
                tempScore = score[i];
                tempCount = count[i];
                tempStart = start[i];
                tempBlocknr = blocknr[i];
                score[i] = score[i + 1];
                count[i] = count[i + 1];
                start[i] = start[i + 1];
                blocknr[i] = blocknr[i + 1];
                score[i + 1] = tempScore;
                count[i + 1] = tempCount;
                start[i + 1] = tempStart;
                blocknr[i + 1] = tempBlocknr;
            }
        }
    }
    while (hadToSwap);
    for (i = 0; i < context.state.block_count; i++)
    {
        totalframes += count[i];
    }

    tempCount = 0;
    Debug(context, 10, "\n\nAfter Sorting - %i\n--------------\n", counter);
    for (i = 0; i < context.state.block_count; i++)
    {
        tempCount += count[i];
        Debug(context, 10, "Block %3i - %.3f\t%6i\t%6i\t%6i\t%3.1f%c\n", blocknr[i], score[i], start[i], context.state.cblock[blocknr[i]].f_end, count[i], ((double)tempCount / (double)totalframes)*100,'%');
    }

    targetCount = (long)(totalframes * percentile);
    i = -1;
    tempCount = 0;
    do
    {
        i++;
        tempCount += count[i];
    }
    while (tempCount < targetCount);
    tempScore = score[i];
    free(score);
    free(count);
    free(percent);
    Debug(context, 6, "The %.2f percentile of %i frames is %.2f\n", (percentile * 100.0), totalframes, tempScore);
    return (tempScore);
}

void OutputLogoHistogram(RecordingContext& context, int buckets)
{
    int		i;
    int		j;
    long	max = 0;
    int		columns = 200;
    double	divisor;
    char stars[256];
    long	counter = 0;

    for (i = 0; i < buckets; i++)
    {
        if (max < context.state.logoHistogram[i])
        {
            max = context.state.logoHistogram[i];
        }
    }

    divisor = (double)columns / (double)max;

    Debug(context, 8, "Logo Histogram - %.5f\n", divisor);

    for (i = 0; i < buckets; i++)
    {
        counter += context.state.logoHistogram[i];
        stars[0] = 0;
        if (context.state.logoHistogram[i] > 0)
        {
            for (j = 0; j <= (int)(context.state.logoHistogram[i] * divisor); j++)
            {
                stars[j] = '*';
            }
            stars[j] = 0;
        }
        Debug(context, 8, "%.3f - %6i - %.5f %s\n", (double)i/buckets, context.state.logoHistogram[i], (double)counter / (double)context.state.frame_count, stars);
    }
}



void OutputbrightHistogram(RecordingContext& context)
{
    int		i;
    int		j;
    long	max = 0;
    int		columns = 200;
    double	divisor;
    long	counter = 0;
    char stars[256];

    for (i = 0; i < 256; i++)
    {
        if (max < context.state.brightHistogram[i])
        {
            max = context.state.brightHistogram[i];
        }
    }

    divisor = (double)columns / (double)max;

    Debug(context, 1, "Show Histogram - %.5f\n", divisor);

    for (i = 0; i < 30; i++)
    {
        counter += context.state.brightHistogram[i];
        stars[0] = 0;
        if (context.state.brightHistogram[i] > 0)
        {
            for (j = 0; j <= (int)(context.state.brightHistogram[i] * divisor); j++)
            {
                stars[j] = '*';
            }
            stars[j] = 0;
        }
        Debug(context, 1, "%3i - %6i - %.5f %s\n", i, context.state.brightHistogram[i], (double)counter / (double)context.state.framesprocessed, stars);
    }
}

void OutputuniformHistogram(RecordingContext& context)
{
    int		i;
    int		j;
    long	max = 0;
    int		columns = 200;
    double	divisor;
    long	counter = 0;
    char stars[256];

    for (i = 0; i < 30; i++)
    {
        if (max < context.state.uniformHistogram[i])
        {
            max = context.state.uniformHistogram[i];
        }
    }

    divisor = (double)columns / (double)max;

    Debug(context, 1, "Show Uniform - %.5f\n", divisor);

    for (i = 0; i < 30; i++)
    {
        counter += context.state.uniformHistogram[i];
        stars[0] = 0;
        if (context.state.uniformHistogram[i] > 0)
        {
            for (j = 0; j <= (int)(context.state.uniformHistogram[i] * divisor); j++)
            {
                stars[j] = '*';
            }
            stars[j] = 0;
        }
        Debug(context, 1, "%3i - %6i - %.5f %s\n", i*UNIFORMSCALE, context.state.uniformHistogram[i], (double)counter / (double)context.state.framesprocessed,stars);
    }
}

void OutputHistogram(RecordingContext& context, int *histogram, int scale, char *title, bool truncate)
{
    int		i;
    int		j;
    long	max = 0;
    int		columns = 70;
    double	divisor;
    long	counter = 0;
    char stars[256];

    for (i = 0; i < (truncate?255:256); i++)
    {
        if (max < histogram[i])
        {
            max = histogram[i];
        }
    }

    divisor = (double)columns / (double)max;

    Debug(context, 8, "Show %s Histogram\n", title);

    for (i = 0; i < 256; i++)
    {
        counter += histogram[i];
        stars[0] = 0;
        if (histogram[i] > 0)
        {
            for (j = 0; j <= (int)(histogram[i] * divisor) && j <= columns; j++)
            {
                stars[j] = '*';
            }
            stars[j] = 0;
        }
        Debug(context, 8, "%3i - %6i - %.5f %s\n", i*scale, histogram[i], (double)counter / (double)context.state.framesprocessed, stars);
    }
}


int FindBlackThreshold(RecordingContext& context, double percentile)
{
    int		i;
    long	tempCount;
    long	targetCount;
    long	totalframes = 0;

    FILE *raw = NULL;
    if (context.settings.output_training) raw = myfopen("black.csv", "a+");

    if (raw) fprintf(raw, "\"%s\"", context.state.inbasename);
    for (i = 0; i < 256; i++)
    {
        totalframes += context.state.brightHistogram[i];
    }

    for (i = 0; i < 35; i++)
    {
        if (raw) fprintf(raw, ",%6.2f", (1000.0*(double)context.state.brightHistogram[i])/totalframes);
    }
    if (raw) fprintf(raw, "\n");
    if (raw) fclose(raw);

    tempCount = 0;
    targetCount = (long)(totalframes * percentile);
    i = -1;
    tempCount = 0;
    do
    {
        i++;
        tempCount += context.state.brightHistogram[i];
    }
    while (tempCount < targetCount);
    return (i);
}

int FindUniformThreshold(RecordingContext& context, double percentile)
{
    int		i;
    long	tempCount;
    long	targetCount;
    long	totalframes = 0;

    FILE *raw = NULL;

    if (context.settings.output_training) raw = myfopen("uniform.csv", "a+");
    if (raw) fprintf(raw, "\"%s\"", context.state.inbasename);

    for (i = 0; i < 256; i++)
    {
        totalframes += context.state.uniformHistogram[i];
    }
    for (i = 0; i < 35; i++)
    {
        if (raw) fprintf(raw, ",%6.2f", (1000.0*(double)context.state.uniformHistogram[i])/totalframes);
    }
    if (raw) fprintf(raw, "\n");
    if (raw) fclose(raw);

    tempCount = 0;
    targetCount = (long)(totalframes * percentile);
    i = -1;
    tempCount = 0;
    do
    {
        i++;
        tempCount += context.state.uniformHistogram[i];
    }
    while (tempCount < targetCount);
    if (i == 0)
        i = 1;
//	while (uniformHistogram[i+1] < uniformHistogram[i])
//		i++;
    return ((i+1)*UNIFORMSCALE);
}

void OutputFrame(RecordingContext& context, int frame_number)
{
    int		x,y;
    FILE*	raw;
    char	array[MAX_PATH];
    sprintf(array, "%.*s%i.frm", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename,frame_number);

    Debug(context, 5, "Sending frame to file\n");
    raw = myfopen(array, "w");
    if (!raw)
    {
        Debug(context, 1, "Could not open frame output file.\n");
        return;
    }

    fprintf(raw, "0;");
    for (x = 0; x < context.state.videowidth; x++)
    {
        fprintf(raw, ";%3i", x);
    }
    fprintf(raw, "\n");

    for (y = 0; y < context.state.height; y++)
    {
        fprintf(raw, "%3i", y);
        for (x = 0; x < context.state.videowidth; x++)
        {
            if (context.state.frame_ptr[y * context.state.width + x] < 30)
                fprintf(raw, ";   ");
            else
                fprintf(raw, ";%3i", context.state.frame_ptr[y * context.state.width + x]);

        }
        fprintf(raw, "\n");
    }
    fclose(raw);
}

int FindFrameWithPts(RecordingContext& context, double t)
{
    int mx,mn;
    mx = context.state.frame_count;
    mn = 1;
    if (context.state.frame) {
    while( mx > mn+1) {
        if (t < context.state.frame[(mx+mn)/2].pts) {
            mx = (mx+mn+0.5)/2;
        } else if (t > context.state.frame[(mx+mn)/2].pts) {
            mn = (mx+mn+0.5)/2;
        } else
            return((mx+mn+0.5)/2);
    }
    return((mx+mn)/2);
    } else
        return(t * context.settings.fps);
}

int InputReffer(RecordingContext& context, const char *extension, int setfps)
{
    int		i;
    long	j;
    int		k, pk;
    double fpos = 0.0, fneg = 0.0, total = 0.0;
    double	t=0;
    enum {both_show,both_commercial, only_reffer, only_commercial} state;
    char	line[2048];
    char	split[256];
    char	array[MAX_PATH];
    FILE*	raw;
    int		x;
    int		col;
    bool	lineProcessed;
    int     frames = 0;
    char	co,re;
    FILE*    raw2=NULL;

    sprintf(array, "%.*s%s", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename,extension);
    raw = myfopen(array, "r");
    if (!raw)
    {
        if (context.settings.output_live)
            goto noreffer;
        return(0);
    }

    fgets(line, sizeof(line), raw); // Read first line

    frames = 0;
    if (strlen(line) > 27)
        frames = strtol(&line[25], NULL, 10);
    if (setfps)
    {
        if (strlen(line) > 42)
            t = ((double)strtol(&line[42], NULL, 10))/100;
        if (t > 99)
            t = t / 10.0;
        if (t > 0) {
            context.settings.fps = t * 1.00000000000001;
            context.state.avg_fps = context.settings.fps;
        }
        if (t != 59.94)
            context.settings.sage_framenumber_bug = false;
    }
    context.state.reffer_count = -1;
    fgets(line, sizeof(line), raw); // Skip second line
    while (fgets(line, sizeof(line), raw) != NULL && strlen(line) > 1)
    {
        if (line[strlen(line)-1] != '\n')
        {
            strcat(&line[strlen(line)], "\n");
        }
        i = 0;
        x = 0;
        col = 0;
        lineProcessed = false;
        context.state.reffer_count++;
        // Split Line Apart
        while (line[i] != '\0' && i < (int)sizeof(line) && !lineProcessed)
        {
            if (line[i] == ' ' || line[i] == '\t' || line[i] == '\n')
            {
                split[x] = '\0';

                switch (col)
                {
                case 0:
                    context.state.reffer[context.state.reffer_count].start_frame = FindFrameWithPts(context, ((double)strtol(split, NULL, 10))/context.settings.fps);
                    if (context.settings.sage_framenumber_bug) context.state.reffer[context.state.reffer_count].start_frame *= 2;
                    break;

                case 1:
                    context.state.reffer[context.state.reffer_count].end_frame = FindFrameWithPts(context, ((double)strtol(split, NULL, 10))/context.settings.fps);
                    if (context.state.reffer[context.state.reffer_count].end_frame < context.state.reffer[context.state.reffer_count].start_frame)
                    {
                        Debug(context, 0,"Error in .ref file, end < start frame\n");
                        context.state.reffer[context.state.reffer_count].end_frame = context.state.reffer[context.state.reffer_count].start_frame + 10;
                    }
                    if (context.settings.sage_framenumber_bug) context.state.reffer[context.state.reffer_count].end_frame *= 2;
                    lineProcessed = true;
                    break;
                }
                col++;
                x = 0;
                split[0] = '\0';
            }
            else
            {
                split[x] = line[i];
                x++;
            }
            i++;
        }
    }
    fclose(raw);
noreffer:
    if (context.state.reffer_count >= 0)
    {
        if (frames == 0)
            frames = context.state.reffer[context.state.reffer_count].end_frame;
        if (context.state.reffer[context.state.reffer_count].end_frame == context.state.reffer[context.state.reffer_count].start_frame+1 &&
                context.state.reffer[context.state.reffer_count].end_frame == frames)
            context.state.reffer_count--;
    }

    if (extension[1] == 't')
        return(frames);

    sprintf(array, "%.*s.dif", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename);
    raw = myfopen(array, "w");
    if (!raw)
    {
        return(0);
    }


//#ifdef faslpositive_negative
    j = 0;
    i = 0;
    state = both_show;
    k = 0;
    context.state.commercial[context.state.commercial_count+1].end_frame = context.state.commercial[context.state.commercial_count].end_frame + 2 ;
    context.state.commercial[context.state.commercial_count+1].start_frame = context.state.commercial[context.state.commercial_count].end_frame + 1;
    context.state.reffer[context.state.reffer_count+1].end_frame = context.state.commercial[context.state.commercial_count].end_frame + 2 ;
    context.state.reffer[context.state.reffer_count+1].start_frame = context.state.commercial[context.state.commercial_count].end_frame + 1;

    if (context.state.reffer[i].end_frame - context.state.reffer[i].start_frame > 2)
    {
        if (context.settings.output_training>1) raw2 = myfopen("quality.csv", "a+");
        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
        if (raw2) fclose(raw2);
    }

    while ( k < context.state.commercial[context.state.commercial_count].end_frame &&
            (i <= context.state.reffer_count || j <= context.state.commercial_count) )
    {
        pk = k;
        switch(state)
        {
        case both_show:
            if (i <= context.state.reffer_count && j <= context.state.commercial_count && labs(context.state.reffer[i].start_frame-context.state.commercial[j].start_frame) < 40)
            {
                state = both_commercial;
                k = context.state.commercial[j].start_frame;
            }
            else if (i > context.state.reffer_count || (j <= context.state.commercial_count && context.state.commercial[j].start_frame < context.state.reffer[i].start_frame) )
            {
                state = only_commercial;
                k = context.state.commercial[j].start_frame;
            }
            else
            {
                state = only_reffer;
                k = context.state.reffer[i].start_frame;
            }
            break;
        case both_commercial:
            if (i <= context.state.reffer_count && j <= context.state.commercial_count && labs(context.state.reffer[i].end_frame-context.state.commercial[j].end_frame) < 40)
            {
                state = both_show;
                k = context.state.commercial[j].end_frame;
                if (i <= context.state.reffer_count)
                {
                    i++;
                    if (context.state.reffer[i].end_frame - context.state.reffer[i].start_frame > 2)
                    {
                        if (context.settings.output_training > 1) raw2 = myfopen("quality.csv", "a+");
                        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
                        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
                        if (raw2) fclose(raw2);
                    }
                }
                if (j <= context.state.commercial_count) j++;
            }
            else if (i > context.state.reffer_count || (j <= context.state.commercial_count && context.state.commercial[j].end_frame < context.state.reffer[i].end_frame ))
            {
                state = only_reffer;
                k = context.state.commercial[j].end_frame;
                if (j <= context.state.commercial_count) j++;
            }
            else
            {
                state = only_commercial;
                k = context.state.reffer[i].end_frame;
                if (i <= context.state.reffer_count)
                {
                    i++;
                    if (context.state.reffer[i].end_frame - context.state.reffer[i].start_frame > 2)
                    {
                        if (context.settings.output_training > 1) raw2 = myfopen("quality.csv", "a+");
                        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
                        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
                        if (raw2) fclose(raw2);
                    }
                }
            }
            break;
        case only_reffer:
            if (j > context.state.commercial_count || context.state.reffer[i].end_frame < context.state.commercial[j].start_frame)
            {
                state = both_show;
                if (i <= context.state.reffer_count)
                    k = context.state.reffer[i].end_frame;
                else
                    k = context.state.commercial[context.state.commercial_count].end_frame;
                if (i <= context.state.reffer_count)
                {
                    i++;
                    if (context.state.reffer[i].end_frame - context.state.reffer[i].start_frame > 2)
                    {
                        if (context.settings.output_training > 1) raw2 = myfopen("quality.csv", "a+");
                        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
                        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
                        if (raw2) fclose(raw2);
                    }
                }
            }
            else
            {
                state = both_commercial;
                k = context.state.commercial[j].start_frame;
            }
//			fprintf(raw, "False negative at frame %6ld of %6.1f seconds\n", pk , (k - pk)/fps );
            if (context.settings.output_training > 1) raw2 = myfopen("quality.csv", "a+");
            if (raw2) fprintf(raw2, "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, pk, F2L(k, pk), 0.0, 0.0);
            fneg += F2L(k,pk);
            if (raw2) fclose(raw2);
            raw2 = NULL;
            break;
        case only_commercial:
            if (i > context.state.reffer_count || context.state.commercial[j].end_frame < context.state.reffer[i].start_frame)
            {
                state = both_show;
                k = context.state.commercial[j].end_frame;
                if (j <= context.state.commercial_count) j++;
            }
            else
            {
                state = both_commercial;
                k = context.state.reffer[i].start_frame;
            }
//			fprintf(raw, "False positive at frame %6ld of %6.1f seconds\n", pk , (k - pk)/fps );
            if (context.settings.output_training > 1) raw2 = myfopen("quality.csv", "a+");
            if (raw2) fprintf(raw2, "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, pk, 0.0, F2L(k, pk), 0.0);
            fpos += F2L(k, pk);
            if (raw2) fclose(raw2);
            raw2 = NULL;
            break;
        }
    }
    if (context.settings.output_training) raw2 = myfopen("quality.csv", "a+");
    if (raw2) fprintf(raw2, "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, -1, fneg, fpos, total);
    if (raw2) fclose(raw2);

//#else
    j = 0;
    i = 0;
    while ( i <= context.state.reffer_count && j <= context.state.commercial_count )
    {
        k = min(context.state.reffer[i].start_frame, context.state.commercial[j].start_frame);
        if ( context.state.commercial[j].end_frame < context.state.reffer[i].start_frame )
        {
            fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", context.state.commercial[j].start_frame, context.state.commercial[j].end_frame, 0L, 0L, F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame) , F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame));
//			fprintf(raw, "Found %6ld %6ld    Not in reference\n", commercial[j].start_frame, commercial[j].end_frame);
            j++;
        }
        else if ( context.state.commercial[j].start_frame > context.state.reffer[i].end_frame )
        {
            fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", 0L, 0L, context.state.reffer[i].start_frame, context.state.reffer[i].end_frame, -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame) , -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
//			fprintf(raw, "Not found %6ld %6ld\n", reffer[i].start_frame, reffer[i].end_frame);
            i++;
        }
        else
        {
            if (labs(context.state.reffer[i].start_frame-context.state.commercial[j].start_frame) > 40 ||
                    labs(context.state.reffer[i].end_frame-context.state.commercial[j].end_frame) > 40 )
            {
                fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", context.state.commercial[j].start_frame, context.state.commercial[j].end_frame, context.state.reffer[i].start_frame, context.state.reffer[i].end_frame, F2L(context.state.reffer[i].start_frame, context.state.commercial[j].start_frame) , F2L(context.state.commercial[j].end_frame , context.state.reffer[i].end_frame));
            }
            /*
                        if (abs(reffer[i].start_frame-commercial[j].start_frame) > 40 ) {
                            fprintf(raw, "Found %5ld %5ld    Reference %5ld %5ld    ", commercial[j].start_frame, commercial[j].end_frame, reffer[i].start_frame, reffer[i].end_frame);
                            fprintf(raw, "starts at %5ld instead of %5ld\n", commercial[j].start_frame, reffer[i].start_frame);
                        }
                        if (abs(reffer[i].end_frame-commercial[j].end_frame) > 40 ) {
                            fprintf(raw, "Found %5ld %5ld    Reference %5ld %5ld    ", commercial[j].start_frame, commercial[j].end_frame, reffer[i].start_frame, reffer[i].end_frame);
                            fprintf(raw, "ends   at %5ld instead of %5ld\n", commercial[j].end_frame, reffer[i].end_frame);
                        }
            */
            i++;
            j++;
        }
    }
    while (j <= context.state.commercial_count)
    {
        fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", context.state.commercial[j].start_frame, context.state.commercial[j].end_frame, 0L, 0L, F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame) , F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame));
//		fprintf(raw, "Found %6ld %6ld    Not in reference\n", commercial[j].start_frame, commercial[j].end_frame);
        j++;
    }
    while (i <= context.state.reffer_count)
    {
        fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", 0L, 0L, context.state.reffer[i].start_frame, context.state.reffer[i].end_frame, -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame) , -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
//		fprintf(raw, "Not found %6ld %6ld\n", reffer[i].start_frame, reffer[i].end_frame);
        i++;
    }
//#endif
    for (i=0; i<context.state.block_count; i++)
    {
        co = CheckFramesForCommercial(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail);
        re = CheckFramesForReffer(context, context.state.cblock[i].f_start+context.state.cblock[i].b_head,context.state.cblock[i].f_end - context.state.cblock[i].b_tail);
        if (co != re)
        {
            fprintf(raw, "Block %6d has mismatch %c%c with cause %s\n", i,co,re, CauseString(context, context.state.cblock[i].cause));
        }
        context.state.cblock[i].reffer = re;
    }

    fclose(raw);
    return(frames);
}


void OutputAspect(RecordingContext& context)
{
    int		i;
//	long	j;
    char	array[MAX_PATH];
    FILE*	raw;

    if (!context.settings.output_aspect)
        return;

    sprintf(array, "%.*s.aspects", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename);
    raw = myfopen(array, "w");
    if (!raw)
    {
        Debug(context, 1, "Could not open aspect output file.\n");
        return;
    }

    // Print out ar cblock list
    for (i = 0; i < context.state.ar_block_count; i++)
    {
        fprintf(
            raw,
            "%s %4dx%4d %.2f minX=%4d, minY=%4d, maxX=%4d, maxY=%4d\n",
            dblSecondsToStrMinutes(context, F2T(context.state.ar_block[i].start)),
            context.state.ar_block[i].width, context.state.ar_block[i].height,
            context.state.ar_block[i].ar_ratio,
            context.state.ar_block[i].minX, context.state.ar_block[i].minY, context.state.ar_block[i].maxX, context.state.ar_block[i].maxY
        );
    }
    fclose(raw);
}





void OutputBlackArray(RecordingContext& context)
{
    int		i;
#ifdef FRAME_WITH_HISTOGRAM
    int		k;
#endif
//	long	j;
    char	array[MAX_PATH];
    FILE*	raw;

return;

    sprintf(array, "%.*s.black.csv", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename);
//	Debug(5, "Expanding logo blocks into frame array\n");
//	for (i = 0; i < logo_block_count; i++) {
//		for (j = logo_block[i].start; j <= logo_block[i].end; j++) {
//			frame[j].logo_present = true;
//		}
//	}
//	Debug(5, "Expanded logo blocks into frame array\n");
    raw = myfopen(array, "w");
    if (!raw)
    {
        Debug(context, 1, "Could not open raw output file.\n");
        return;
    }
    fprintf(raw, "black,frame,brightness,cause,uniform,volume\n");
    for (i = 1; i < context.state.black_count; i++)
    {
        fprintf(raw, "%i,%ld,%i,%i,%ld,%i\n",
                    i,
                    context.state.black[i].frame,
                    context.state.black[i].brightness,
                    context.state.black[i].cause,
                    context.state.black[i].uniform,
                    context.state.black[i].volume
                   );
    }

    fclose(raw);
}



void OutputFrameArray(RecordingContext& context, bool screenOnly)
{
    int		i;
#ifdef FRAME_WITH_HISTOGRAM
    int		k;
#endif
//	long	j;
    char	array[MAX_PATH];
    FILE*	raw;
    char	lp[10];
    sprintf(array, "%.*s.csv", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename);
//	Debug(5, "Expanding logo blocks into frame array\n");
//	for (i = 0; i < logo_block_count; i++) {
//		for (j = logo_block[i].start; j <= logo_block[i].end; j++) {
//			frame[j].logo_present = true;
//		}
//	}
//	Debug(5, "Expanded logo blocks into frame array\n");
    raw = myfopen(array, "w");
    if (!raw)
    {
        Debug(context, 1, "Could not open raw output file.\n");
        return;
    }
    fprintf(raw, "sep=,\nframe,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,isblack,cutscene, MinX, MaxX, hasBright, Dimcount,PTS,%f",context.settings.fps);
//	for (k = 0; k < 32; k++) {
//		fprintf(raw, ",b%3i", k);
//	}
    fprintf(raw, "\n");



    if (screenOnly)
        Debug(context, 1, "Frame\tBlack\tBrightness\tS_Change\tS_Change Perc\tLogo Present\t%i\n", context.state.frame_count);
    for (i = 1; i < context.state.frame_count; i++)
    {
        if (screenOnly)
        {
            printf("%i\t%i\t%i\t%s\tHistogram\n", i, context.state.frame[i].brightness, context.state.frame[i].schange_percent, lp);
        }
        else
        {
            fprintf(raw, "%i,%i,%i,%i,%i,%i,%i,%i,%f,%f,%i,%i,%i,%i,%i,%i,%f,%i,%i",
                    i, context.state.frame[i].brightness, context.state.frame[i].schange_percent*5, context.state.frame[i].logo_present,
                    context.state.frame[i].uniform, context.state.frame[i].volume,  context.state.frame[i].minY,context.state.frame[i].maxY,context.state.frame[i].ar_ratio,
                    context.state.frame[i].currentGoodEdge, context.state.frame[i].isblack,context.state.frame[i].cutscenematch,
                    context.state.frame[i].minX, context.state.frame[i].maxX, context.state.frame[i].hasBright, context.state.frame[i].dimCount, context.state.frame[i].pts,
                    context.state.frame[i].cur_segment, context.state.frame[i].audio_channels
                   );
#ifdef FRAME_WITH_HISTOGRAM
            for (k = 0; k < 32; k++)
            {
                fprintf(raw, ",%i", frame[i].histogram[k]);
            }
#endif
            fprintf(raw, "\n");
        }
    }

    fclose(raw);
}

