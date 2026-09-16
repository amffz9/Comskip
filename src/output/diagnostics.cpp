#include "detection/reference_comparison.h"
#include "platform/utf8_paths.h"
#include "input/file_stream.h"
#include "input/reference_file.h"
#include "exit_requested.h"
#include "legacy_detection.h"
#include "output/diagnostics.h"
#include "output/csv_field.h"
#include "weighted_scores.h"
#include "search_path.h"
#include "checked_format.h"
#include <cstdlib>

void FindIniFile(RecordingContext& context)
{
    const auto* environment = std::getenv("PATH");
    const std::string_view paths = environment ? environment : "";
    const auto working_directory = std::filesystem::current_path();
#ifdef _WIN32
    constexpr char separator = ';';
    constexpr std::string_view executable = "comskip.exe";
#else
    constexpr char separator = ':';
    constexpr std::string_view executable = "comskip";
#endif
    const auto search = [&](std::string_view name, auto& destination) {
        const auto found = comskip::platform::find_in_search_path(name, paths, working_directory, separator);
        if (found) {
            const auto bytes = found->u8string();
            comskip::checked_format(destination, "%s", reinterpret_cast<const char*>(bytes.c_str()));
            Debug(context, 1, "Path for %s: %s\n", std::string(name).c_str(), destination.c_str());
        } else {
            destination.clear();
            Debug(context, 1, "%s not found\n", std::string(name).c_str());
        }
    };
    search("comskip.ini", context.state.inifilename);
    search("comskip.dictionary", context.state.dictfilename);
    search(executable, context.state.exefilename);
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

    if (raw.get()) fprintf(raw.get(), "%s", comskip::output::csv_field(context.state.inbasename).c_str());
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
    if (raw.get()) fprintf(raw.get(), "%s", comskip::output::csv_field(context.state.inbasename).c_str());

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
    std::string array;
    array = comskip::platform::path_to_utf8(comskip::platform::path_from_utf8(context.state.logfilename).replace_extension()) + std::to_string(frame_number) + ".frm";

    Debug(context, 5, "Sending frame to file\n");
    raw.reset(myfopen(array.c_str(), "w"));
    if (!raw.get())
    {
        Debug(context, 1, "%s", context.translator.text("diagnostics_frame_open_failed"));
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
    int k;
    double fpos = 0.0, fneg = 0.0, total = 0.0;
    double	t=0;
    comskip::platform::FilePtr raw;
    int frames = 0;
    char co,re;
    comskip::platform::FilePtr raw2;
    if (!extension || std::string_view(extension).size() < 2)
        throw std::invalid_argument("Missing reference filename extension");
    comskip::detection::validate_intervals(context.state.commercial, context.state.commercial_count);
    comskip::detection::validate_intervals(context.state.reffer, context.state.reffer_count);
    auto basename = std::string(context.state.logfilename);
    if (basename.ends_with(".log") || basename.ends_with(".txt")) basename.resize(basename.size() - 4);
    const auto reference_name = basename + extension;
    raw.reset(myfopen(reference_name.c_str(), "r"));
    if (!raw) {
        if (!context.settings.output_live) return 0;
    } else {
        comskip::input::FileStreamBuffer buffer(raw.get());
        std::istream source(&buffer);
        const auto document = comskip::input::read_reference_file(source);
        std::vector<Legacy_reffer_entry> reference;
        reference.reserve(document.intervals.size());
        frames = document.declared_frames;
        if (setfps && document.frames_per_second) {
            t = *document.frames_per_second;
            context.settings.fps = t * 1.00000000000001;
            context.state.avg_fps = context.settings.fps;
            if (t != 59.94) context.settings.sage_framenumber_bug = false;
        }
        for (const auto& interval : document.intervals) {
            Legacy_reffer_entry entry{};
            entry.start_frame = FindFrameWithPts(context, interval.start_frame / context.settings.fps);
            entry.end_frame = FindFrameWithPts(context, interval.end_frame / context.settings.fps);
            if (context.settings.sage_framenumber_bug) entry.start_frame *= 2;
            if (entry.end_frame < entry.start_frame) {
                Debug(context, 0, "%s", context.translator.text("diagnostics_reference_reversed"));
                entry.end_frame = entry.start_frame + 10;
            }
            if (context.settings.sage_framenumber_bug) entry.end_frame *= 2;
            reference.push_back(entry);
        }
        context.state.reffer = std::move(reference);
        context.state.reffer_count = static_cast<int>(context.state.reffer.size()) - 1;
        raw.reset();
    }

    if (context.state.reffer_count >= 0)
    {
        if (frames == 0)
            frames = context.state.reffer[context.state.reffer_count].end_frame;
        if (context.state.reffer[context.state.reffer_count].end_frame == context.state.reffer[context.state.reffer_count].start_frame+1 &&
                context.state.reffer[context.state.reffer_count].end_frame == frames)
            comskip::detection::erase_interval(context.state.reffer, context.state.reffer_count,
                                              context.state.reffer_count);
    }

    if (extension[1] == 't')
        return(frames);

    const auto difference_name = basename + ".dif";
    raw.reset(myfopen(difference_name.c_str(), "w"));
    if (!raw.get())
    {
        return(0);
    }


    using comskip::output::CommercialInterval;
    const auto intervals = [](const auto& storage, int last) {
        if (last < -1 || last >= static_cast<int>(std::size(storage)))
            throw std::out_of_range("Reference comparison count exceeds stored intervals");
        std::vector<CommercialInterval> values;
        values.reserve(static_cast<std::size_t>(last + 1));
        for (int index = 0; index <= last; ++index)
            values.push_back({storage[index].start_frame, storage[index].end_frame});
        return values;
    };
    const auto references = intervals(context.state.reffer, context.state.reffer_count);
    const auto commercials = intervals(context.state.commercial, context.state.commercial_count);
    const auto events = comskip::detection::compare_reference_intervals(references, commercials);
    for (const auto& event : events) {
        const auto start = static_cast<long>(event.interval.start_frame);
        const auto end = static_cast<long>(event.interval.end_frame);
        const auto duration = F2L(end, start);
        if (event.kind == comskip::detection::ReferenceEventKind::reference_duration) {
            total += duration;
            if (context.settings.output_training > 1) {
                raw2.reset(myfopen("quality.csv", "a+"));
                if (raw2) fprintf(raw2.get(), "%s, %6ld, %6.1f, %6.1f, %6.1f\n", comskip::output::csv_field(context.state.inbasename).c_str(), start, 0.0, 0.0, duration);
            }
        } else {
            const bool missed = event.kind == comskip::detection::ReferenceEventKind::false_negative;
            if (missed) fneg += duration; else fpos += duration;
            if (context.settings.output_training > 1) {
                raw2.reset(myfopen("quality.csv", "a+"));
                if (raw2) fprintf(raw2.get(), "%s, %6ld, %6.1f, %6.1f, %6.1f\n", comskip::output::csv_field(context.state.inbasename).c_str(), start, missed ? duration : 0.0, missed ? 0.0 : duration, 0.0);
            }
        }
        raw2.reset();
    }
    if (context.settings.output_training) raw2.reset(myfopen("quality.csv", "a+"));
    if (raw2) fprintf(raw2.get(), "%s, %6d, %6.1f, %6.1f, %6.1f\n", comskip::output::csv_field(context.state.inbasename).c_str(), -1, fneg, fpos, total);
    raw2.reset();
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
    std::string array;
    comskip::platform::FilePtr raw;

    if (!context.settings.output_aspect)
        return;

    array = comskip::platform::path_to_utf8(comskip::platform::path_from_utf8(context.state.logfilename).replace_extension(".aspects"));
    raw.reset(myfopen(array.c_str(), "w"));
    if (!raw.get())
    {
        Debug(context, 1, "%s", context.translator.text("diagnostics_aspect_open_failed"));
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
    std::string array;
    comskip::platform::FilePtr raw;

return;

    array = comskip::platform::path_to_utf8(comskip::platform::path_from_utf8(context.state.logfilename).replace_extension(".black.csv"));
//	Debug(5, "Expanding logo blocks into frame array\n");
//	for (i = 0; i < logo_block_count; i++) {
//		for (j = logo_block[i].start; j <= logo_block[i].end; j++) {
//			frame[j].logo_present = true;
//		}
//	}
//	Debug(5, "Expanded logo blocks into frame array\n");
    raw.reset(myfopen(array.c_str(), "w"));
    if (!raw.get())
    {
        Debug(context, 1, "%s", context.translator.text("diagnostics_raw_open_failed"));
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
    std::string array;
    comskip::platform::FilePtr raw;
    array = comskip::platform::path_to_utf8(comskip::platform::path_from_utf8(context.state.logfilename).replace_extension(".csv"));
//	Debug(5, "Expanding logo blocks into frame array\n");
//	for (i = 0; i < logo_block_count; i++) {
//		for (j = logo_block[i].start; j <= logo_block[i].end; j++) {
//			frame[j].logo_present = true;
//		}
//	}
//	Debug(5, "Expanded logo blocks into frame array\n");
    raw.reset(myfopen(array.c_str(), "w"));
    if (!raw.get())
    {
        Debug(context, 1, "%s", context.translator.text("diagnostics_raw_open_failed"));
        return;
    }
    fprintf(raw.get(), "sep=,\nframe,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,isblack,cutscene, MinX, MaxX, hasBright, Dimcount,PTS,%f",context.settings.fps);
//	for (k = 0; k < 32; k++) {
//		fprintf(raw, ",b%3i", k);
//	}
    fprintf(raw.get(), "\n");



    if (screenOnly)
        Debug(context, 1, "Frame\tBlack\tBrightness\tS_Change\tS_Change Perc\tLogo Present\t%i\n", context.state.frame_count);
    // Both decoded input and CSV replay count real observations inclusively.
    const int last_observation = context.state.frame_count;
    if (last_observation < 0 || static_cast<std::size_t>(last_observation) >= context.state.frame.size())
        throw std::out_of_range("CSV observations exceed the frame buffer");
    for (i = 1; i <= last_observation; i++)
    {
        if (screenOnly)
        {
            printf("%i\t%i\t%i\t%i\tHistogram\n", i, context.state.frame[i].brightness,
                   context.state.frame[i].schange_percent, context.state.frame[i].logo_present);
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

