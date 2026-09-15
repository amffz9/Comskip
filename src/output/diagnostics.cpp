#include "exit_requested.h"
#include "legacy_detection.h"

void FindIniFile(void)
{
#ifdef _WIN32
    char	searchinifile[] = "comskip.ini";
    char	searchexefile[] = "comskip.exe";
    char	searchdictfile[] = "comskip.dictionary";
    char	envvar[] = "PATH";
    _searchenv(searchinifile, envvar, inifilename);
    if (*inifilename != '\0')
    {
        Debug(1, "Path for %s: %s\n", searchinifile, inifilename);
    }
    else
    {
        Debug(1, "%s not found\n", searchinifile);
    }

    _searchenv(searchdictfile, envvar, dictfilename);
    if (*dictfilename != '\0')
    {
        Debug(1, "Path for %s: %s\n", searchdictfile, dictfilename);
    }
    else
    {
        Debug(1, "%s not found\n", searchdictfile);
    }

    _searchenv(searchexefile, envvar, exefilename);
    if (*exefilename != '\0')
    {
        Debug(1, "Path for %s: %s\n", searchexefile, exefilename);
    }
    else
    {
        Debug(1, "%s not found\n", searchexefile);
    }
#endif
}

double FindScoreThreshold(double percentile)
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
    score = static_cast<double *>( malloc(sizeof(cblock[0].score) * block_count) );
    count = static_cast<long *>( malloc(sizeof(long) * block_count) );
    start = static_cast<long *>( malloc(sizeof(long) * block_count) );
    blocknr = static_cast<int *>( malloc(sizeof(int) * block_count) );
    percent = static_cast<double *>( malloc(sizeof(double) * block_count) );
    if ((score == NULL) || (count == NULL) || (start == NULL) || (blocknr == NULL) || (percent == NULL))
    {
        Debug(1, "Could not allocate memory.  Exiting program.\n");
        comskip::request_exit(21);
    }

    counter = 0;
    for (i = 0; i < block_count; i++)
    {
        blocknr[i] = i;
        score[i] = cblock[i].score;
        count[i] = cblock[i].f_end - cblock[i].f_start + 1;
        start[i] = cblock[i].f_start;
    }

    do
    {
        hadToSwap = false;
        counter++;
        for (i = 0; i < block_count - 1; i++)
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
    for (i = 0; i < block_count; i++)
    {
        totalframes += count[i];
    }

    tempCount = 0;
    Debug(10, "\n\nAfter Sorting - %i\n--------------\n", counter);
    for (i = 0; i < block_count; i++)
    {
        tempCount += count[i];
        Debug(10, "Block %3i - %.3f\t%6i\t%6i\t%6i\t%3.1f%c\n", blocknr[i], score[i], start[i], cblock[blocknr[i]].f_end, count[i], ((double)tempCount / (double)totalframes)*100,'%');
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
    Debug(6, "The %.2f percentile of %i frames is %.2f\n", (percentile * 100.0), totalframes, tempScore);
    return (tempScore);
}

void OutputLogoHistogram(int buckets)
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
        if (max < logoHistogram[i])
        {
            max = logoHistogram[i];
        }
    }

    divisor = (double)columns / (double)max;

    Debug(8, "Logo Histogram - %.5f\n", divisor);

    for (i = 0; i < buckets; i++)
    {
        counter += logoHistogram[i];
        stars[0] = 0;
        if (logoHistogram[i] > 0)
        {
            for (j = 0; j <= (int)(logoHistogram[i] * divisor); j++)
            {
                stars[j] = '*';
            }
            stars[j] = 0;
        }
        Debug(8, "%.3f - %6i - %.5f %s\n", (double)i/buckets, logoHistogram[i], (double)counter / (double)frame_count, stars);
    }
}



void OutputbrightHistogram(void)
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
        if (max < brightHistogram[i])
        {
            max = brightHistogram[i];
        }
    }

    divisor = (double)columns / (double)max;

    Debug(1, "Show Histogram - %.5f\n", divisor);

    for (i = 0; i < 30; i++)
    {
        counter += brightHistogram[i];
        stars[0] = 0;
        if (brightHistogram[i] > 0)
        {
            for (j = 0; j <= (int)(brightHistogram[i] * divisor); j++)
            {
                stars[j] = '*';
            }
            stars[j] = 0;
        }
        Debug(1, "%3i - %6i - %.5f %s\n", i, brightHistogram[i], (double)counter / (double)framesprocessed, stars);
    }
}

void OutputuniformHistogram(void)
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
        if (max < uniformHistogram[i])
        {
            max = uniformHistogram[i];
        }
    }

    divisor = (double)columns / (double)max;

    Debug(1, "Show Uniform - %.5f\n", divisor);

    for (i = 0; i < 30; i++)
    {
        counter += uniformHistogram[i];
        stars[0] = 0;
        if (uniformHistogram[i] > 0)
        {
            for (j = 0; j <= (int)(uniformHistogram[i] * divisor); j++)
            {
                stars[j] = '*';
            }
            stars[j] = 0;
        }
        Debug(1, "%3i - %6i - %.5f %s\n", i*UNIFORMSCALE, uniformHistogram[i], (double)counter / (double)framesprocessed,stars);
    }
}

void OutputHistogram(int *histogram, int scale, char *title, bool truncate)
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

    Debug(8, "Show %s Histogram\n", title);

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
        Debug(8, "%3i - %6i - %.5f %s\n", i*scale, histogram[i], (double)counter / (double)framesprocessed, stars);
    }
}


int FindBlackThreshold(double percentile)
{
    int		i;
    long	tempCount;
    long	targetCount;
    long	totalframes = 0;

    FILE *raw = NULL;
    if (output_training) raw = myfopen("black.csv", "a+");

    if (raw) fprintf(raw, "\"%s\"", inbasename);
    for (i = 0; i < 256; i++)
    {
        totalframes += brightHistogram[i];
    }

    for (i = 0; i < 35; i++)
    {
        if (raw) fprintf(raw, ",%6.2f", (1000.0*(double)brightHistogram[i])/totalframes);
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
        tempCount += brightHistogram[i];
    }
    while (tempCount < targetCount);
    return (i);
}

int FindUniformThreshold(double percentile)
{
    int		i;
    long	tempCount;
    long	targetCount;
    long	totalframes = 0;

    FILE *raw = NULL;

    if (output_training) raw = myfopen("uniform.csv", "a+");
    if (raw) fprintf(raw, "\"%s\"", inbasename);

    for (i = 0; i < 256; i++)
    {
        totalframes += uniformHistogram[i];
    }
    for (i = 0; i < 35; i++)
    {
        if (raw) fprintf(raw, ",%6.2f", (1000.0*(double)uniformHistogram[i])/totalframes);
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
        tempCount += uniformHistogram[i];
    }
    while (tempCount < targetCount);
    if (i == 0)
        i = 1;
//	while (uniformHistogram[i+1] < uniformHistogram[i])
//		i++;
    return ((i+1)*UNIFORMSCALE);
}

void OutputFrame(int frame_number)
{
    int		x,y;
    FILE*	raw;
    char	array[MAX_PATH];
    sprintf(array, "%.*s%i.frm", (int)(strlen(logfilename) - 4), logfilename,frame_number);

    Debug(5, "Sending frame to file\n");
    raw = myfopen(array, "w");
    if (!raw)
    {
        Debug(1, "Could not open frame output file.\n");
        return;
    }

    fprintf(raw, "0;");
    for (x = 0; x < videowidth; x++)
    {
        fprintf(raw, ";%3i", x);
    }
    fprintf(raw, "\n");

    for (y = 0; y < height; y++)
    {
        fprintf(raw, "%3i", y);
        for (x = 0; x < videowidth; x++)
        {
            if (frame_ptr[y * width + x] < 30)
                fprintf(raw, ";   ");
            else
                fprintf(raw, ";%3i", frame_ptr[y * width + x]);

        }
        fprintf(raw, "\n");
    }
    fclose(raw);
}

int FindFrameWithPts(double t)
{
    int mx,mn;
    mx = frame_count;
    mn = 1;
    if (frame) {
    while( mx > mn+1) {
        if (t < frame[(mx+mn)/2].pts) {
            mx = (mx+mn+0.5)/2;
        } else if (t > frame[(mx+mn)/2].pts) {
            mn = (mx+mn+0.5)/2;
        } else
            return((mx+mn+0.5)/2);
    }
    return((mx+mn)/2);
    } else
        return(t * fps);
}

int InputReffer(const char *extension, int setfps)
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

    sprintf(array, "%.*s%s", (int)(strlen(logfilename) - 4), logfilename,extension);
    raw = myfopen(array, "r");
    if (!raw)
    {
        if (output_live)
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
            fps = t * 1.00000000000001;
            avg_fps = fps;
        }
        if (t != 59.94)
            sage_framenumber_bug = false;
    }
    reffer_count = -1;
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
        reffer_count++;
        // Split Line Apart
        while (line[i] != '\0' && i < (int)sizeof(line) && !lineProcessed)
        {
            if (line[i] == ' ' || line[i] == '\t' || line[i] == '\n')
            {
                split[x] = '\0';

                switch (col)
                {
                case 0:
                    reffer[reffer_count].start_frame = FindFrameWithPts(((double)strtol(split, NULL, 10))/fps);
                    if (sage_framenumber_bug) reffer[reffer_count].start_frame *= 2;
                    break;

                case 1:
                    reffer[reffer_count].end_frame = FindFrameWithPts(((double)strtol(split, NULL, 10))/fps);
                    if (reffer[reffer_count].end_frame < reffer[reffer_count].start_frame)
                    {
                        Debug(0,"Error in .ref file, end < start frame\n");
                        reffer[reffer_count].end_frame = reffer[reffer_count].start_frame + 10;
                    }
                    if (sage_framenumber_bug) reffer[reffer_count].end_frame *= 2;
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
    if (reffer_count >= 0)
    {
        if (frames == 0)
            frames = reffer[reffer_count].end_frame;
        if (reffer[reffer_count].end_frame == reffer[reffer_count].start_frame+1 &&
                reffer[reffer_count].end_frame == frames)
            reffer_count--;
    }

    if (extension[1] == 't')
        return(frames);

    sprintf(array, "%.*s.dif", (int)(strlen(logfilename) - 4), logfilename);
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
    commercial[commercial_count+1].end_frame = commercial[commercial_count].end_frame + 2 ;
    commercial[commercial_count+1].start_frame = commercial[commercial_count].end_frame + 1;
    reffer[reffer_count+1].end_frame = commercial[commercial_count].end_frame + 2 ;
    reffer[reffer_count+1].start_frame = commercial[commercial_count].end_frame + 1;

    if (reffer[i].end_frame - reffer[i].start_frame > 2)
    {
        if (output_training>1) raw2 = myfopen("quality.csv", "a+");
        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", inbasename, reffer[i].start_frame, 0.0, 0.0, F2L(reffer[i].end_frame, reffer[i].start_frame));
        total += F2L(reffer[i].end_frame, reffer[i].start_frame);
        if (raw2) fclose(raw2);
    }

    while ( k < commercial[commercial_count].end_frame &&
            (i <= reffer_count || j <= commercial_count) )
    {
        pk = k;
        switch(state)
        {
        case both_show:
            if (i <= reffer_count && j <= commercial_count && labs(reffer[i].start_frame-commercial[j].start_frame) < 40)
            {
                state = both_commercial;
                k = commercial[j].start_frame;
            }
            else if (i > reffer_count || (j <= commercial_count && commercial[j].start_frame < reffer[i].start_frame) )
            {
                state = only_commercial;
                k = commercial[j].start_frame;
            }
            else
            {
                state = only_reffer;
                k = reffer[i].start_frame;
            }
            break;
        case both_commercial:
            if (i <= reffer_count && j <= commercial_count && labs(reffer[i].end_frame-commercial[j].end_frame) < 40)
            {
                state = both_show;
                k = commercial[j].end_frame;
                if (i <= reffer_count)
                {
                    i++;
                    if (reffer[i].end_frame - reffer[i].start_frame > 2)
                    {
                        if (output_training > 1) raw2 = myfopen("quality.csv", "a+");
                        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", inbasename, reffer[i].start_frame, 0.0, 0.0, F2L(reffer[i].end_frame, reffer[i].start_frame));
                        total += F2L(reffer[i].end_frame, reffer[i].start_frame);
                        if (raw2) fclose(raw2);
                    }
                }
                if (j <= commercial_count) j++;
            }
            else if (i > reffer_count || (j <= commercial_count && commercial[j].end_frame < reffer[i].end_frame ))
            {
                state = only_reffer;
                k = commercial[j].end_frame;
                if (j <= commercial_count) j++;
            }
            else
            {
                state = only_commercial;
                k = reffer[i].end_frame;
                if (i <= reffer_count)
                {
                    i++;
                    if (reffer[i].end_frame - reffer[i].start_frame > 2)
                    {
                        if (output_training > 1) raw2 = myfopen("quality.csv", "a+");
                        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", inbasename, reffer[i].start_frame, 0.0, 0.0, F2L(reffer[i].end_frame, reffer[i].start_frame));
                        total += F2L(reffer[i].end_frame, reffer[i].start_frame);
                        if (raw2) fclose(raw2);
                    }
                }
            }
            break;
        case only_reffer:
            if (j > commercial_count || reffer[i].end_frame < commercial[j].start_frame)
            {
                state = both_show;
                if (i <= reffer_count)
                    k = reffer[i].end_frame;
                else
                    k = commercial[commercial_count].end_frame;
                if (i <= reffer_count)
                {
                    i++;
                    if (reffer[i].end_frame - reffer[i].start_frame > 2)
                    {
                        if (output_training > 1) raw2 = myfopen("quality.csv", "a+");
                        if (raw2) fprintf(raw2, "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", inbasename, reffer[i].start_frame, 0.0, 0.0, F2L(reffer[i].end_frame, reffer[i].start_frame));
                        total += F2L(reffer[i].end_frame, reffer[i].start_frame);
                        if (raw2) fclose(raw2);
                    }
                }
            }
            else
            {
                state = both_commercial;
                k = commercial[j].start_frame;
            }
//			fprintf(raw, "False negative at frame %6ld of %6.1f seconds\n", pk , (k - pk)/fps );
            if (output_training > 1) raw2 = myfopen("quality.csv", "a+");
            if (raw2) fprintf(raw2, "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", inbasename, pk, F2L(k, pk), 0.0, 0.0);
            fneg += F2L(k,pk);
            if (raw2) fclose(raw2);
            raw2 = NULL;
            break;
        case only_commercial:
            if (i > reffer_count || commercial[j].end_frame < reffer[i].start_frame)
            {
                state = both_show;
                k = commercial[j].end_frame;
                if (j <= commercial_count) j++;
            }
            else
            {
                state = both_commercial;
                k = reffer[i].start_frame;
            }
//			fprintf(raw, "False positive at frame %6ld of %6.1f seconds\n", pk , (k - pk)/fps );
            if (output_training > 1) raw2 = myfopen("quality.csv", "a+");
            if (raw2) fprintf(raw2, "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", inbasename, pk, 0.0, F2L(k, pk), 0.0);
            fpos += F2L(k, pk);
            if (raw2) fclose(raw2);
            raw2 = NULL;
            break;
        }
    }
    if (output_training) raw2 = myfopen("quality.csv", "a+");
    if (raw2) fprintf(raw2, "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", inbasename, -1, fneg, fpos, total);
    if (raw2) fclose(raw2);

//#else
    j = 0;
    i = 0;
    while ( i <= reffer_count && j <= commercial_count )
    {
        k = min(reffer[i].start_frame, commercial[j].start_frame);
        if ( commercial[j].end_frame < reffer[i].start_frame )
        {
            fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", commercial[j].start_frame, commercial[j].end_frame, 0L, 0L, F2L(commercial[j].end_frame, commercial[j].start_frame) , F2L(commercial[j].end_frame, commercial[j].start_frame));
//			fprintf(raw, "Found %6ld %6ld    Not in reference\n", commercial[j].start_frame, commercial[j].end_frame);
            j++;
        }
        else if ( commercial[j].start_frame > reffer[i].end_frame )
        {
            fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", 0L, 0L, reffer[i].start_frame, reffer[i].end_frame, -F2L(reffer[i].end_frame, reffer[i].start_frame) , -F2L(reffer[i].end_frame, reffer[i].start_frame));
//			fprintf(raw, "Not found %6ld %6ld\n", reffer[i].start_frame, reffer[i].end_frame);
            i++;
        }
        else
        {
            if (labs(reffer[i].start_frame-commercial[j].start_frame) > 40 ||
                    labs(reffer[i].end_frame-commercial[j].end_frame) > 40 )
            {
                fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", commercial[j].start_frame, commercial[j].end_frame, reffer[i].start_frame, reffer[i].end_frame, F2L(reffer[i].start_frame, commercial[j].start_frame) , F2L(commercial[j].end_frame , reffer[i].end_frame));
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
    while (j <= commercial_count)
    {
        fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", commercial[j].start_frame, commercial[j].end_frame, 0L, 0L, F2L(commercial[j].end_frame, commercial[j].start_frame) , F2L(commercial[j].end_frame, commercial[j].start_frame));
//		fprintf(raw, "Found %6ld %6ld    Not in reference\n", commercial[j].start_frame, commercial[j].end_frame);
        j++;
    }
    while (i <= reffer_count)
    {
        fprintf(raw, "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", 0L, 0L, reffer[i].start_frame, reffer[i].end_frame, -F2L(reffer[i].end_frame, reffer[i].start_frame) , -F2L(reffer[i].end_frame, reffer[i].start_frame));
//		fprintf(raw, "Not found %6ld %6ld\n", reffer[i].start_frame, reffer[i].end_frame);
        i++;
    }
//#endif
    for (i=0; i<block_count; i++)
    {
        co = CheckFramesForCommercial(cblock[i].f_start+cblock[i].b_head,cblock[i].f_end - cblock[i].b_tail);
        re = CheckFramesForReffer(cblock[i].f_start+cblock[i].b_head,cblock[i].f_end - cblock[i].b_tail);
        if (co != re)
        {
            fprintf(raw, "Block %6d has mismatch %c%c with cause %s\n", i,co,re, CauseString(cblock[i].cause));
        }
        cblock[i].reffer = re;
    }

    fclose(raw);
    return(frames);
}


void OutputAspect(void)
{
    int		i;
//	long	j;
    char	array[MAX_PATH];
    FILE*	raw;

    if (!output_aspect)
        return;

    sprintf(array, "%.*s.aspects", (int)(strlen(logfilename) - 4), logfilename);
    raw = myfopen(array, "w");
    if (!raw)
    {
        Debug(1, "Could not open aspect output file.\n");
        return;
    }

    // Print out ar cblock list
    for (i = 0; i < ar_block_count; i++)
    {
        fprintf(
            raw,
            "%s %4dx%4d %.2f minX=%4d, minY=%4d, maxX=%4d, maxY=%4d\n",
            dblSecondsToStrMinutes(F2T(ar_block[i].start)),
            ar_block[i].width, ar_block[i].height,
            ar_block[i].ar_ratio,
            ar_block[i].minX, ar_block[i].minY, ar_block[i].maxX, ar_block[i].maxY
        );
    }
    fclose(raw);
}





void OutputBlackArray()
{
    int		i;
#ifdef FRAME_WITH_HISTOGRAM
    int		k;
#endif
//	long	j;
    char	array[MAX_PATH];
    FILE*	raw;

return;

    sprintf(array, "%.*s.black.csv", (int)(strlen(logfilename) - 4), logfilename);
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
        Debug(1, "Could not open raw output file.\n");
        return;
    }
    fprintf(raw, "black,frame,brightness,cause,uniform,volume\n");
    for (i = 1; i < black_count; i++)
    {
        fprintf(raw, "%i,%ld,%i,%i,%ld,%i\n",
                    i,
                    black[i].frame,
                    black[i].brightness,
                    black[i].cause,
                    black[i].uniform,
                    black[i].volume
                   );
    }

    fclose(raw);
}



void OutputFrameArray(bool screenOnly)
{
    int		i;
#ifdef FRAME_WITH_HISTOGRAM
    int		k;
#endif
//	long	j;
    char	array[MAX_PATH];
    FILE*	raw;
    char	lp[10];
    sprintf(array, "%.*s.csv", (int)(strlen(logfilename) - 4), logfilename);
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
        Debug(1, "Could not open raw output file.\n");
        return;
    }
    fprintf(raw, "sep=,\nframe,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,isblack,cutscene, MinX, MaxX, hasBright, Dimcount,PTS,%f",fps);
//	for (k = 0; k < 32; k++) {
//		fprintf(raw, ",b%3i", k);
//	}
    fprintf(raw, "\n");



    if (screenOnly)
        Debug(1, "Frame\tBlack\tBrightness\tS_Change\tS_Change Perc\tLogo Present\t%i\n", frame_count);
    for (i = 1; i < frame_count; i++)
    {
        if (screenOnly)
        {
            printf("%i\t%i\t%i\t%s\tHistogram\n", i, frame[i].brightness, frame[i].schange_percent, lp);
        }
        else
        {
            fprintf(raw, "%i,%i,%i,%i,%i,%i,%i,%i,%f,%f,%i,%i,%i,%i,%i,%i,%f,%i,%i",
                    i, frame[i].brightness, frame[i].schange_percent*5, frame[i].logo_present,
                    frame[i].uniform, frame[i].volume,  frame[i].minY,frame[i].maxY,frame[i].ar_ratio,
                    frame[i].currentGoodEdge, frame[i].isblack,frame[i].cutscenematch,
                    frame[i].minX, frame[i].maxX, frame[i].hasBright, frame[i].dimCount, frame[i].pts,
                    frame[i].cur_segment, frame[i].audio_channels
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

