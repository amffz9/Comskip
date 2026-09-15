#include "exit_requested.h"
#include "legacy_detection.h"
#include "weighted_scores.h"

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
    using comskip::detection::WeightedScore;
    std::vector<WeightedScore> samples;
    if (context.state.block_count < 0 ||
        static_cast<std::size_t>(context.state.block_count) > std::size(context.state.cblock))
        throw std::invalid_argument("Score threshold has an invalid block count");
    samples.reserve(context.state.block_count);
    for (int i = 0; i < context.state.block_count; ++i) {
        const auto& block = context.state.cblock[i];
        if (block.f_start < 0 || block.f_end < block.f_start)
            throw std::invalid_argument("Score threshold has an invalid frame interval");
        samples.push_back({block.score, static_cast<std::uint64_t>(block.f_end) -
            static_cast<std::uint64_t>(block.f_start) + 1});
    }
    const auto threshold = comskip::detection::weighted_score_threshold(samples, percentile);
    if (!threshold) throw std::invalid_argument("Cannot select a score threshold from invalid samples or percentile");
    std::uint64_t frames = 0;
    for (const auto& sample : samples) frames += sample.frames;
    Debug(context, 6, "The %.2f percentile of %llu frames is %.2f\n",
        percentile * 100, static_cast<unsigned long long>(frames), *threshold);
    return *threshold;
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

    comskip::platform::FilePtr raw;
    if (context.settings.output_training) raw.reset(myfopen("black.csv", "a+"));

    if (raw.get()) fprintf(raw.get(), "\"%s\"", context.state.inbasename);
    for (i = 0; i < 256; i++)
    {
        totalframes += context.state.brightHistogram[i];
    }

    for (i = 0; i < 35; i++)
    {
        if (raw.get()) fprintf(raw.get(), ",%6.2f", (1000.0*(double)context.state.brightHistogram[i])/totalframes);
    }
    if (raw.get()) fprintf(raw.get(), "\n");
    if (raw.get()) raw.reset();

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

    comskip::platform::FilePtr raw;

    if (context.settings.output_training) raw.reset(myfopen("uniform.csv", "a+"));
    if (raw.get()) fprintf(raw.get(), "\"%s\"", context.state.inbasename);

    for (i = 0; i < 256; i++)
    {
        totalframes += context.state.uniformHistogram[i];
    }
    for (i = 0; i < 35; i++)
    {
        if (raw.get()) fprintf(raw.get(), ",%6.2f", (1000.0*(double)context.state.uniformHistogram[i])/totalframes);
    }
    if (raw.get()) fprintf(raw.get(), "\n");
    if (raw.get()) raw.reset();

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
    comskip::platform::FilePtr raw;
    char	array[MAX_PATH];
    sprintf(array, "%.*s%i.frm", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename,frame_number);

    Debug(context, 5, "Sending frame to file\n");
    raw.reset(myfopen(array, "w"));
    if (!raw.get())
    {
        Debug(context, 1, "Could not open frame output file.\n");
        return;
    }

    fprintf(raw.get(), "0;");
    for (x = 0; x < context.state.videowidth; x++)
    {
        fprintf(raw.get(), ";%3i", x);
    }
    fprintf(raw.get(), "\n");

    for (y = 0; y < context.state.height; y++)
    {
        fprintf(raw.get(), "%3i", y);
        for (x = 0; x < context.state.videowidth; x++)
        {
            if (context.state.frame_ptr[y * context.state.width + x] < 30)
                fprintf(raw.get(), ";   ");
            else
                fprintf(raw.get(), ";%3i", context.state.frame_ptr[y * context.state.width + x]);

        }
        fprintf(raw.get(), "\n");
    }
    raw.reset();
}

int FindFrameWithPts(RecordingContext& context, double t)
{
    int mx,mn;
    mx = context.state.frame_count;
    mn = 1;
    if (!context.state.frame.empty()) {
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
    comskip::platform::FilePtr raw;
    int		x;
    int		col;
    bool	lineProcessed;
    int     frames = 0;
    char	co,re;
    comskip::platform::FilePtr raw2;

    sprintf(array, "%.*s%s", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename,extension);
    raw.reset(myfopen(array, "r"));
    if (!raw.get())
    {
        if (context.settings.output_live)
            goto noreffer;
        return(0);
    }

    fgets(line, sizeof(line), raw.get()); // Read first line

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
    fgets(line, sizeof(line), raw.get()); // Skip second line
    while (fgets(line, sizeof(line), raw.get()) != NULL && strlen(line) > 1)
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
    raw.reset();
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
    raw.reset(myfopen(array, "w"));
    if (!raw.get())
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
        if (context.settings.output_training>1) raw2.reset(myfopen("quality.csv", "a+"));
        if (raw2.get()) fprintf(raw2.get(), "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
        if (raw2.get()) raw2.reset();
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
                        if (context.settings.output_training > 1) raw2.reset(myfopen("quality.csv", "a+"));
                        if (raw2.get()) fprintf(raw2.get(), "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
                        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
                        if (raw2.get()) raw2.reset();
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
                        if (context.settings.output_training > 1) raw2.reset(myfopen("quality.csv", "a+"));
                        if (raw2.get()) fprintf(raw2.get(), "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
                        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
                        if (raw2.get()) raw2.reset();
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
                        if (context.settings.output_training > 1) raw2.reset(myfopen("quality.csv", "a+"));
                        if (raw2.get()) fprintf(raw2.get(), "\"%s\", %6ld, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, context.state.reffer[i].start_frame, 0.0, 0.0, F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
                        total += F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame);
                        if (raw2.get()) raw2.reset();
                    }
                }
            }
            else
            {
                state = both_commercial;
                k = context.state.commercial[j].start_frame;
            }
//			fprintf(raw, "False negative at frame %6ld of %6.1f seconds\n", pk , (k - pk)/fps );
            if (context.settings.output_training > 1) raw2.reset(myfopen("quality.csv", "a+"));
            if (raw2.get()) fprintf(raw2.get(), "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, pk, F2L(k, pk), 0.0, 0.0);
            fneg += F2L(k,pk);
            if (raw2.get()) raw2.reset();
            raw2.reset();
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
            if (context.settings.output_training > 1) raw2.reset(myfopen("quality.csv", "a+"));
            if (raw2.get()) fprintf(raw2.get(), "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, pk, 0.0, F2L(k, pk), 0.0);
            fpos += F2L(k, pk);
            if (raw2.get()) raw2.reset();
            raw2.reset();
            break;
        }
    }
    if (context.settings.output_training) raw2.reset(myfopen("quality.csv", "a+"));
    if (raw2.get()) fprintf(raw2.get(), "\"%s\", %6d, %6.1f, %6.1f, %6.1f\n", context.state.inbasename, -1, fneg, fpos, total);
    if (raw2.get()) raw2.reset();

//#else
    j = 0;
    i = 0;
    while ( i <= context.state.reffer_count && j <= context.state.commercial_count )
    {
        k = min(context.state.reffer[i].start_frame, context.state.commercial[j].start_frame);
        if ( context.state.commercial[j].end_frame < context.state.reffer[i].start_frame )
        {
            fprintf(raw.get(), "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", context.state.commercial[j].start_frame, context.state.commercial[j].end_frame, 0L, 0L, F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame) , F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame));
//			fprintf(raw, "Found %6ld %6ld    Not in reference\n", commercial[j].start_frame, commercial[j].end_frame);
            j++;
        }
        else if ( context.state.commercial[j].start_frame > context.state.reffer[i].end_frame )
        {
            fprintf(raw.get(), "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", 0L, 0L, context.state.reffer[i].start_frame, context.state.reffer[i].end_frame, -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame) , -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
//			fprintf(raw, "Not found %6ld %6ld\n", reffer[i].start_frame, reffer[i].end_frame);
            i++;
        }
        else
        {
            if (labs(context.state.reffer[i].start_frame-context.state.commercial[j].start_frame) > 40 ||
                    labs(context.state.reffer[i].end_frame-context.state.commercial[j].end_frame) > 40 )
            {
                fprintf(raw.get(), "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", context.state.commercial[j].start_frame, context.state.commercial[j].end_frame, context.state.reffer[i].start_frame, context.state.reffer[i].end_frame, F2L(context.state.reffer[i].start_frame, context.state.commercial[j].start_frame) , F2L(context.state.commercial[j].end_frame , context.state.reffer[i].end_frame));
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
        fprintf(raw.get(), "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", context.state.commercial[j].start_frame, context.state.commercial[j].end_frame, 0L, 0L, F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame) , F2L(context.state.commercial[j].end_frame, context.state.commercial[j].start_frame));
//		fprintf(raw, "Found %6ld %6ld    Not in reference\n", commercial[j].start_frame, commercial[j].end_frame);
        j++;
    }
    while (i <= context.state.reffer_count)
    {
        fprintf(raw.get(), "Found %6ld %6ld    Reference %6ld %6ld    Difference %+6.1f    %+6.1f\n", 0L, 0L, context.state.reffer[i].start_frame, context.state.reffer[i].end_frame, -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame) , -F2L(context.state.reffer[i].end_frame, context.state.reffer[i].start_frame));
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
            fprintf(raw.get(), "Block %6d has mismatch %c%c with cause %s\n", i,co,re, CauseString(context, context.state.cblock[i].cause));
        }
        context.state.cblock[i].reffer = re;
    }

    raw.reset();
    return(frames);
}


void OutputAspect(RecordingContext& context)
{
    int		i;
//	long	j;
    char	array[MAX_PATH];
    comskip::platform::FilePtr raw;

    if (!context.settings.output_aspect)
        return;

    sprintf(array, "%.*s.aspects", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename);
    raw.reset(myfopen(array, "w"));
    if (!raw.get())
    {
        Debug(context, 1, "Could not open aspect output file.\n");
        return;
    }

    // Print out ar cblock list
    for (i = 0; i < context.state.ar_block_count; i++)
    {
        fprintf(
            raw.get(),
            "%s %4dx%4d %.2f minX=%4d, minY=%4d, maxX=%4d, maxY=%4d\n",
            dblSecondsToStrMinutes(context, F2T(context.state.ar_block[i].start)),
            context.state.ar_block[i].width, context.state.ar_block[i].height,
            context.state.ar_block[i].ar_ratio,
            context.state.ar_block[i].minX, context.state.ar_block[i].minY, context.state.ar_block[i].maxX, context.state.ar_block[i].maxY
        );
    }
    raw.reset();
}





void OutputBlackArray(RecordingContext& context)
{
    int		i;
#ifdef FRAME_WITH_HISTOGRAM
    int		k;
#endif
//	long	j;
    char	array[MAX_PATH];
    comskip::platform::FilePtr raw;

return;

    sprintf(array, "%.*s.black.csv", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename);
//	Debug(5, "Expanding logo blocks into frame array\n");
//	for (i = 0; i < logo_block_count; i++) {
//		for (j = logo_block[i].start; j <= logo_block[i].end; j++) {
//			frame[j].logo_present = true;
//		}
//	}
//	Debug(5, "Expanded logo blocks into frame array\n");
    raw.reset(myfopen(array, "w"));
    if (!raw.get())
    {
        Debug(context, 1, "Could not open raw output file.\n");
        return;
    }
    fprintf(raw.get(), "black,frame,brightness,cause,uniform,volume\n");
    for (i = 1; i < context.state.black_count; i++)
    {
        fprintf(raw.get(), "%i,%ld,%i,%i,%ld,%i\n",
                    i,
                    context.state.black[i].frame,
                    context.state.black[i].brightness,
                    context.state.black[i].cause,
                    context.state.black[i].uniform,
                    context.state.black[i].volume
                   );
    }

    raw.reset();
}



void OutputFrameArray(RecordingContext& context, bool screenOnly)
{
    int		i;
#ifdef FRAME_WITH_HISTOGRAM
    int		k;
#endif
//	long	j;
    char	array[MAX_PATH];
    comskip::platform::FilePtr raw;
    char	lp[10];
    sprintf(array, "%.*s.csv", (int)(strlen(context.state.logfilename) - 4), context.state.logfilename);
//	Debug(5, "Expanding logo blocks into frame array\n");
//	for (i = 0; i < logo_block_count; i++) {
//		for (j = logo_block[i].start; j <= logo_block[i].end; j++) {
//			frame[j].logo_present = true;
//		}
//	}
//	Debug(5, "Expanded logo blocks into frame array\n");
    raw.reset(myfopen(array, "w"));
    if (!raw.get())
    {
        Debug(context, 1, "Could not open raw output file.\n");
        return;
    }
    fprintf(raw.get(), "sep=,\nframe,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,isblack,cutscene, MinX, MaxX, hasBright, Dimcount,PTS,%f",context.settings.fps);
//	for (k = 0; k < 32; k++) {
//		fprintf(raw, ",b%3i", k);
//	}
    fprintf(raw.get(), "\n");



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
            fprintf(raw.get(), "%i,%i,%i,%i,%i,%i,%i,%i,%f,%f,%i,%i,%i,%i,%i,%i,%f,%i,%i",
                    i, context.state.frame[i].brightness, context.state.frame[i].schange_percent*5, context.state.frame[i].logo_present,
                    context.state.frame[i].uniform, context.state.frame[i].volume,  context.state.frame[i].minY,context.state.frame[i].maxY,context.state.frame[i].ar_ratio,
                    context.state.frame[i].currentGoodEdge, context.state.frame[i].isblack,context.state.frame[i].cutscenematch,
                    context.state.frame[i].minX, context.state.frame[i].maxX, context.state.frame[i].hasBright, context.state.frame[i].dimCount, context.state.frame[i].pts,
                    context.state.frame[i].cur_segment, context.state.frame[i].audio_channels
                   );
#ifdef FRAME_WITH_HISTOGRAM
            for (k = 0; k < 32; k++)
            {
                fprintf(raw.get(), ",%i", frame[i].histogram[k]);
            }
#endif
            fprintf(raw.get(), "\n");
        }
    }

    raw.reset();
}

