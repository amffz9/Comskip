#include "diagnostic.h"
#include "detection/reference_comparison.h"
#include "platform/utf8_paths.h"
#include "input/file_stream.h"
#include "input/reference_file.h"
#include "exit_requested.h"
#include "legacy_detection.h"
#include "output/diagnostics.h"
#include "output/csv_field.h"
#include "output/frame_csv.h"
#include "output/histogram_report.h"
#include "output/checked_file.h"
#include "weighted_scores.h"
#include "search_path.h"
#include "checked_format.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

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
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::score_threshold_invalid_block_count);
    samples.reserve(context.state.block_count);
    for (int i = 0; i < context.state.block_count; ++i) {
        const auto& block = context.state.cblock[i];
        if (block.f_start < 0 || block.f_end < block.f_start)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::score_threshold_invalid_frame_interval);
        samples.push_back({block.score, static_cast<std::uint64_t>(block.f_end) -
            static_cast<std::uint64_t>(block.f_start) + 1});
    }
    const auto threshold = comskip::detection::weighted_score_threshold(samples, percentile);
    if (!threshold) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::cannot_select_score_threshold);
    std::uint64_t frames = 0;
    for (const auto& sample : samples) frames += sample.frames;
    Debug(context, 6, "The %.2f percentile of %llu frames is %.2f\n",
        percentile * 100, static_cast<unsigned long long>(frames), *threshold);
    return *threshold;
}

void OutputLogoHistogram(RecordingContext& context,
                         std::span<const std::uint64_t> histogram,
                         std::uint64_t denominator)
{
    constexpr std::size_t columns = 200;
    const auto maximum = histogram.empty()
        ? std::uint64_t{0}
        : *std::ranges::max_element(histogram);
    const auto divisor = maximum == 0 ? 0.0 : static_cast<double>(columns) / maximum;
    std::uint64_t counter = 0;

    Debug(context, 8, "Logo Histogram - %.5f\n", divisor);

    for (std::size_t i = 0; i < histogram.size(); ++i) {
        counter += histogram[i];
        const auto star_count = histogram[i] == 0
            ? std::size_t{0}
            : std::min(columns + 1,
                       static_cast<std::size_t>(histogram[i] * divisor) + 1);
        const std::string stars(star_count, '*');
        const auto fraction = denominator == 0 ? 0.0
            : static_cast<double>(counter) / static_cast<double>(denominator);
        Debug(context, 8, "%.3f - %6llu - %.5f %s\n",
              static_cast<double>(i) / histogram.size(),
              static_cast<unsigned long long>(histogram[i]), fraction, stars.c_str());
    }
}



void OutputbrightHistogram(RecordingContext& context)
{
    const auto report=comskip::output::make_histogram_report<int>(context.state.brightHistogram,
        256,30,1,200,context.state.framesprocessed>0 ? context.state.framesprocessed : 0);
    if (!report) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_histogram_report);
    Debug(context,1,"Show Histogram - %.5f\n",report->divisor);
    for (const auto& row : report->rows)
        Debug(context,1,"%3lld - %6llu - %.5f %s\n",static_cast<long long>(row.label),
            static_cast<unsigned long long>(row.count),row.cumulative_fraction,row.stars.c_str());
}

void OutputuniformHistogram(RecordingContext& context)
{
    const auto report=comskip::output::make_histogram_report<int>(context.state.uniformHistogram,
        30,30,UNIFORMSCALE,200,context.state.framesprocessed>0 ? context.state.framesprocessed : 0);
    if (!report) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_histogram_report);
    Debug(context,1,"Show Uniform - %.5f\n",report->divisor);
    for (const auto& row : report->rows)
        Debug(context,1,"%3lld - %6llu - %.5f %s\n",static_cast<long long>(row.label),
            static_cast<unsigned long long>(row.count),row.cumulative_fraction,row.stars.c_str());
}

void OutputHistogram(RecordingContext& context, int *histogram, int scale, char *title, bool truncate)
{
    if (!histogram) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_histogram_report);
    Debug(context, 8, "Show %s Histogram\n", title);
    const auto report=comskip::output::make_histogram_report<int>({histogram,256},truncate?255:256,
        256,scale,70,context.state.framesprocessed>0 ? context.state.framesprocessed : 0);
    if (!report) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_histogram_report);
    for (const auto& row : report->rows)
        Debug(context,8,"%3lld - %6llu - %.5f %s\n",static_cast<long long>(row.label),
            static_cast<unsigned long long>(row.count),row.cumulative_fraction,row.stars.c_str());
}


int FindBlackThreshold(RecordingContext& context, double percentile)
{
    int		i;
    std::int64_t tempCount;
    std::int64_t targetCount;
    std::int64_t totalframes = 0;

    for (i = 0; i < 256; i++)
    {
        if (context.state.brightHistogram[i] < 0)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
                comskip::diagnostics::Code::invalid_histogram_report);
        totalframes += context.state.brightHistogram[i];
    }

    if (totalframes <= 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
            comskip::diagnostics::Code::invalid_histogram_report);

    comskip::platform::FilePtr raw;
    if (context.settings.output_training) raw.reset(myfopen("black.csv", "a+"));
    if (raw.get()) fprintf(raw.get(), "%s", comskip::output::csv_field(context.state.inbasename).c_str());

    for (i = 0; i < 35; i++)
    {
        if (raw.get()) fprintf(raw.get(), ",%6.2f", (1000.0*(double)context.state.brightHistogram[i])/totalframes);
    }
    if (raw.get()) fprintf(raw.get(), "\n");
    if (raw.get()) raw.reset();

    tempCount = 0;
    targetCount = static_cast<std::int64_t>(totalframes * percentile);
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
    std::int64_t tempCount;
    std::int64_t targetCount;
    std::int64_t totalframes = 0;

    for (i = 0; i < 256; i++)
    {
        if (context.state.uniformHistogram[i] < 0)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
                comskip::diagnostics::Code::invalid_histogram_report);
        totalframes += context.state.uniformHistogram[i];
    }
    if (totalframes <= 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
            comskip::diagnostics::Code::invalid_histogram_report);

    comskip::platform::FilePtr raw;
    if (context.settings.output_training) raw.reset(myfopen("uniform.csv", "a+"));
    if (raw.get()) fprintf(raw.get(), "%s", comskip::output::csv_field(context.state.inbasename).c_str());

    for (i = 0; i < 35; i++)
    {
        if (raw.get()) fprintf(raw.get(), ",%6.2f", (1000.0*(double)context.state.uniformHistogram[i])/totalframes);
    }
    if (raw.get()) fprintf(raw.get(), "\n");
    if (raw.get()) raw.reset();

    tempCount = 0;
    targetCount = static_cast<std::int64_t>(totalframes * percentile);
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
    const auto path = comskip::platform::path_to_utf8(
        comskip::platform::path_from_utf8(context.state.logfilename).replace_extension()) +
        std::to_string(frame_number) + ".frm";

    Debug(context, 5, "Sending frame to file\n");
    auto file = comskip::platform::own_file(myfopen(path.c_str(), "w"));
    if (!file)
    {
        Debug(context, 1, "%s", context.translator.text("diagnostics_frame_open_failed"));
        return;
    }

    comskip::output::checked_fprintf(*file, path, "0;");
    for (int x = 0; x < context.state.videowidth; ++x)
    {
        comskip::output::checked_fprintf(*file, path, ";%3i", x);
    }
    comskip::output::checked_fprintf(*file, path, "\n");

    for (int y = 0; y < context.state.height; ++y)
    {
        comskip::output::checked_fprintf(*file, path, "%3i", y);
        for (int x = 0; x < context.state.videowidth; ++x)
        {
            if (context.state.frame_ptr[y * context.state.width + x] < 30)
                comskip::output::checked_fprintf(*file, path, ";   ");
            else
                comskip::output::checked_fprintf(*file, path, ";%3i", context.state.frame_ptr[y * context.state.width + x]);

        }
        comskip::output::checked_fprintf(*file, path, "\n");
    }
    comskip::output::checked_close(file, path);
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
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::missing_reference_filename_extension);
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
        source.exceptions(std::ios::badbit);
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
            throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::reference_comparison_count_exceeds_storage);
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
    const int last_observation = context.state.frame_count;
    if (last_observation < 0 || static_cast<std::size_t>(last_observation) >= context.state.frame.size())
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::csv_observations_exceed_frame_buffer);
    const auto observations=std::span(context.state.frame).subspan(1,static_cast<std::size_t>(last_observation));
    if (screenOnly) {
        Debug(context,1,"Frame\tBrightness\tS_Change\tLogo Present\t%i\n",last_observation);
        for (std::size_t index=0; index<observations.size(); ++index)
            printf("%zu\t%i\t%i\t%i\tHistogram\n",index+1,observations[index].brightness,
                observations[index].schange_percent,observations[index].logo_present);
        return;
    }
    try {
        comskip::output::validate_frame_csv(observations,{context.settings.fps});
    } catch (const std::invalid_argument&) {
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
            comskip::diagnostics::Code::invalid_frame_csv_output);
    }
    const auto path=comskip::platform::path_from_utf8(context.state.logfilename).replace_extension(".csv");
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    const auto path_text=comskip::platform::path_to_utf8(path);
    if (!output)
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_open,{path_text});
    try {
        comskip::output::write_frame_csv(output,observations,{context.settings.fps});
        output.close();
        if (!output) throw std::ios_base::failure("Failed closing frame CSV output");
    } catch (const std::ios_base::failure&) {
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_write,{path_text});
    }
}

