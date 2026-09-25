#include "../localization/diagnostic.h"
#include "app/debug.h"
#include "app/recording_context.h"
#include "detection/detection_methods.h"
#include "detection/detector_runtime.h"
#include "detection/frame_causes.h"
#include "detection/frame_timestamps.h"
#include "detection/logo_detection.h"
#include "detection/storage.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include "frame_mask.h"
#include "scene_sampling.h"
#include "scene_brightness.h"
#include "cutscene_file.h"
#include "platform/utf8_paths.h"
#include <fstream>
#include <stdexcept>
#include <format>
#include <string>
#include <string_view>
#include <span>
#include <utility>

namespace {

inline constexpr int own_histogram_height = 256;
inline constexpr int uniform_scale = 100;
inline constexpr int maximum_aspect_ratios = 1000;
inline constexpr double undefined_aspect_ratio = 0.0;
inline constexpr double aspect_ratio_trend = 0.8;

[[nodiscard]] constexpr int cause_flag(comskip::detection::FrameCause value) noexcept
{
    return comskip::detection::cause_value(value);
}

template <typename... Args>
void SceneDebug(RecordingContext& context, int level, std::string_view key, Args&&... args)
{
    Debug(context, level, context.translator.format(key, std::forward<Args>(args)...));
}

[[nodiscard]] std::size_t BrightnessIndex(int brightness)
{
    const auto index = comskip::detection::brightness_histogram_index(
        brightness, own_histogram_height);
    if (!index) {
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(
            comskip::diagnostics::Code::invalid_scene_brightness,
            {std::to_string(brightness), std::to_string(own_histogram_height)});
    }
    return *index;
}

} // namespace

void ProcessARInfoInit(RecordingContext& context, int minY, int maxY, int minX, int maxX)
{
    double pictureHeight = maxY - minY;
    double pictureWidth = maxX - minX;

    if (minX <= context.settings.border) minX = 1;
    if (minY <= context.settings.border) minY = 1;
    if (maxY >= context.state.height - context.settings.border) maxY = context.state.height;
    if (maxX >= context.state.videowidth - context.settings.border) maxX = context.state.videowidth;

    /*
    ar_width = width;
    if (ar_width < maxY + minY)
        ar_width = static_cast<int>((maxY + minY) * 1.3);
    */

    context.state.last_ar_ratio = pictureWidth / pictureHeight;
    context.state.last_ar_ratio = ceil(context.state.last_ar_ratio * context.state.ar_rounding) / context.state.ar_rounding;
    context.state.ar_ratio_trend = context.state.last_ar_ratio;
    if (context.state.last_ar_ratio < 0.5 || context.state.last_ar_ratio > 3.0)
        context.state.last_ar_ratio = undefined_aspect_ratio;
//	lastAR = (last_ar_ratio <= ar_split);
    context.state.ar_ratio_start = context.state.framenum_real;
    context.state.ar_block[context.state.ar_block_count].start = context.state.framenum_real;
    context.state.ar_block[context.state.ar_block_count].width = context.state.videowidth;
    context.state.ar_block[context.state.ar_block_count].height = context.state.height;
    context.state.ar_block[context.state.ar_block_count].minX = minX;
    context.state.ar_block[context.state.ar_block_count].minY = minY;
    context.state.ar_block[context.state.ar_block_count].maxX = maxX;
    context.state.ar_block[context.state.ar_block_count].maxY = maxY;
//			ar_block[ar_block_count].ar = lastAR;
    context.state.ar_block[context.state.ar_block_count].ar_ratio = context.state.last_ar_ratio;
//	if (framearray) frame[frame_count].ar_ratio = last_ar_ratio;;
    SceneDebug(context, 4, "scene_aspect_bounds", context.state.ar_ratio_start,
               std::format("{:.2f}", context.state.ar_ratio_trend), minY, maxY, minX, maxX);

//	Debug(4, "\nFirst Frame\nFrame: %i\tMinY: %i\tMaxY: %i\tRatio: %.2f\n", framenum_real, minY, maxY, last_ar_ratio);
}

void ProcessARInfo(RecordingContext& context, int minY, int maxY, int minX, int maxX)
{
    int		pictureHeight, pictureWidth;
    int		hi,i;
    double	cur_ar_ratio;

    if (minX <= context.settings.border) minX = 1;
    if (minY <= context.settings.border) minY = 1;
    if (maxY >= context.state.height - context.settings.border) maxY = context.state.height;
    if (maxX >= context.state.videowidth - context.settings.border) maxX = context.state.videowidth;


    const auto bottom_mask_rows = comskip::detection::effective_mask_rows(
        context.settings.ticker_tape, context.settings.ticker_tape_percentage,
        static_cast<std::size_t>(context.state.height));
    const auto top_mask_rows = comskip::detection::effective_mask_rows(
        context.settings.top_ticker_tape, context.settings.top_ticker_tape_percentage,
        static_cast<std::size_t>(context.state.height));
    if (bottom_mask_rows != 0 || top_mask_rows != 0 || (
                abs((context.state.height - maxY) - (minY)) < 13 + (minY )/15  &&  // discard for no simetrical check
                abs((context.state.videowidth  - maxX) - (minX)) < 13 + (minX )/15  &&  // discard for no simetrical check
                minY < context.state.height / 4 &&
                minX < context.state.videowidth / 4)
       )   // check if simetrical
    {

        pictureHeight = maxY - minY;
        pictureWidth = maxX - minX;
        cur_ar_ratio = static_cast<double>(pictureWidth) / pictureHeight;
        cur_ar_ratio = ceil(cur_ar_ratio * context.state.ar_rounding) / context.state.ar_rounding;
        if (cur_ar_ratio > 3.0 || cur_ar_ratio < 0.5)
            cur_ar_ratio = undefined_aspect_ratio;

        hi = static_cast<int>((cur_ar_ratio - 0.5) * 100);
        if (hi >= 0 && hi < maximum_aspect_ratios)
        {
            context.state.ar_histogram[hi].frames += 1;
            context.state.ar_histogram[hi].ar_ratio = cur_ar_ratio;
        }


        if (cur_ar_ratio - context.state.last_ar_ratio > context.settings.ar_delta || cur_ar_ratio - context.state.last_ar_ratio < -context.settings.ar_delta)
        {
            if (cur_ar_ratio - context.state.ar_ratio_trend < context.settings.ar_delta && cur_ar_ratio - context.state.ar_ratio_trend > -context.settings.ar_delta)
            {
                // Same ratio as previous trend
                context.state.ar_ratio_trend_counter++;
                if (context.state.ar_ratio_trend_counter / context.settings.fps > aspect_ratio_trend)
                {
                    context.state.last_ar_ratio = context.state.ar_ratio_trend;
                    context.state.ar_ratio_trend_counter = 0;
                    context.state.ar_misratio_trend_counter = 0;
                    if (comskip::detection::method_enabled(context.settings.commDetectMethod,
                                                           comskip::detection::DetectionMethod::aspect_ratio))
                    {

                        context.state.ar_block[context.state.ar_block_count].end = context.state.ar_ratio_start-1;
                        context.state.ar_block_count++;
                        InitializeARBlockArray(context, context.state.ar_block_count);
                        context.state.ar_block[context.state.ar_block_count].start = context.state.ar_ratio_start;
                        context.state.ar_block[context.state.ar_block_count].ar_ratio = context.state.ar_ratio_trend;
                        context.state.ar_block[context.state.ar_block_count].volume = 0;
                        context.state.ar_block[context.state.ar_block_count].width = context.state.videowidth;
                        context.state.ar_block[context.state.ar_block_count].height = context.state.height;
                        context.state.ar_block[context.state.ar_block_count].minX = minX;
                        context.state.ar_block[context.state.ar_block_count].minY = minY;
                        context.state.ar_block[context.state.ar_block_count].maxX = maxX;
                        context.state.ar_block[context.state.ar_block_count].maxY = maxY;
                    }
                    SceneDebug(context, 9, "scene_aspect_bounds_tabbed", context.state.ar_ratio_start,
                               std::format("{:.2f}", context.state.ar_ratio_trend), minY, maxY, minX, maxX);
                    context.state.last_ar_ratio = context.state.ar_ratio_trend;
                    if (context.state.framearray)
                    {
                        for (i = context.state.ar_ratio_start; i < context.state.framenum_real; i++)
                        {
                            context.state.frame[i].ar_ratio = context.state.last_ar_ratio;
                        }
                    }
                }
            }
            else  						// Other ratio as previous trend
            {
                context.state.ar_ratio_trend = cur_ar_ratio;
                context.state.ar_ratio_trend_counter = 0;
                context.state.ar_ratio_start = context.state.framenum_real;
            }
            context.state.ar_misratio_trend_counter++;
        }
        else  							// Same ratio as previous frame
        {
            context.state.ar_ratio_trend_counter = 0;
            context.state.ar_ratio_start = context.state.framenum_real;
            context.state.ar_misratio_trend_counter = 0;
            context.state.ar_misratio_start = context.state.framenum_real;
        }

    }
    else
    {
        // Unreliable ratio
//		ar_ratio_trend = cur_ar_ratio;
        context.state.ar_ratio_trend_counter = 0;
        context.state.ar_ratio_start = context.state.framenum_real;
        /*
          //	if (framearray) frame[frame_count].minY = 0;
            //	if (framearray) frame[frame_count].maxY = height;
            //	Debug(9, "Frame: %i\tAsimetrical\tMinY: %i\tMaxY: %i\n", framenum_real, minY, maxY);
                ar_ratio_trend_counter = 0;
                ar_ratio_start = framenum_real;
                ar_misratio_trend_counter++;
                if (framearray) frame[frame_count].ar_ratio = 0.0;
        */

    }
    /*
        if (last_ar_ratio == 0) {
            ar_misratio_trend_counter = 0;

        } else {
    */
    if (context.state.ar_misratio_trend_counter > 3*context.settings.fps && context.state.last_ar_ratio != undefined_aspect_ratio)
    {
        context.state.last_ar_ratio = context.state.ar_ratio_trend = undefined_aspect_ratio;
        if (comskip::detection::method_enabled(context.settings.commDetectMethod,
                                                comskip::detection::DetectionMethod::aspect_ratio))
        {
            context.state.ar_block[context.state.ar_block_count].end = context.state.framenum_real - 3*static_cast<int>(context.settings.fps) -1;
            context.state.ar_block_count++;
            InitializeARBlockArray(context, context.state.ar_block_count);
            context.state.ar_block[context.state.ar_block_count].start = context.state.framenum_real - 3*static_cast<int>(context.settings.fps);
            context.state.ar_block[context.state.ar_block_count].ar_ratio = context.state.ar_ratio_trend;
            context.state.ar_block[context.state.ar_block_count].volume = 0;
            context.state.ar_block[context.state.ar_block_count].width = context.state.videowidth;
            context.state.ar_block[context.state.ar_block_count].height = context.state.height;
            context.state.ar_block[context.state.ar_block_count].minX = minX;
            context.state.ar_block[context.state.ar_block_count].minY = minY;
            context.state.ar_block[context.state.ar_block_count].maxX = maxX;
            context.state.ar_block[context.state.ar_block_count].maxY = maxY;
        }
        SceneDebug(context, 9, "scene_aspect_vertical_bounds", context.state.ar_ratio_start,
                   std::format("{:.2f}", context.state.ar_ratio_trend), minY, maxY);
        context.state.last_ar_ratio = context.state.ar_ratio_trend;
    }
//	}

}

void ProcessACInfoInit(RecordingContext& context, int audio_channels)
{
//    audio_channels_start = framenum_real;
    context.state.ac_block[context.state.ac_block_count].start = context.state.framenum_real;
    context.state.ac_block[context.state.ac_block_count].audio_channels = audio_channels;
    context.state.last_audio_channels = audio_channels;
    SceneDebug(context, 4, "scene_audio_channels", context.state.framenum_real,
               std::format("{:2}", audio_channels));
}

void ProcessACInfo(RecordingContext& context, int audio_channels)
{
    if (context.state.last_audio_channels == audio_channels )
        return;
    context.state.ac_block[context.state.ac_block_count].end = context.state.framenum_real;
    context.state.ac_block_count++;
    InitializeACBlockArray(context, context.state.ac_block_count);
    context.state.last_audio_channels = audio_channels;
//    audio_channels_start = framenum_real;
    context.state.ac_block[context.state.ac_block_count].start = context.state.framenum_real;
    context.state.ac_block[context.state.ac_block_count].audio_channels = audio_channels;
    context.state.last_audio_channels = audio_channels;
    SceneDebug(context, 4, "scene_audio_channels", context.state.framenum_real,
               std::format("{:2}", audio_channels));
}

int MatchCutScene(RecordingContext& context, std::span<const unsigned char> cutscene)
{
    int x,y,d;
    int delta = 0;
    int step = 4;
    int c=0;
    if (context.state.width > 800) step = 8;
    if (context.state.width < 400) step = 2;
    if (context.state.width < 200) step = 1;
    for (y = context.settings.border; y < (context.state.height - context.settings.border); y += step)
    {
        for (x = context.settings.border; x < (context.state.videowidth - context.settings.border); x += step)
        {
            if (c < comskip::detection::maximum_cutscene_pixels)
            {
                d = static_cast<int>(context.state.frame_ptr[y * context.state.width + x]) - static_cast<int>(cutscene[c]);
                if (d > context.settings.edge_level_threshold || d < -context.settings.edge_level_threshold)
                    delta += 1;
            }
            c++;
        }
    }
    return(delta);
}

void RecordCutScene(RecordingContext& context, int frame_count, int brightness)
{
    std::vector<std::uint8_t> cs;
    cs.reserve(comskip::detection::maximum_cutscene_pixels);
    int c;
    int x,y;
    int step = 4;

    if (context.state.width > 800) step = 8;
    if (context.state.width < 400) step = 2;
    if (context.state.width < 200) step = 1;
    c = 0;
    for (y = context.settings.border; y < (context.state.height - context.settings.border); y += step)
    {
        for (x = context.settings.border; x < (context.state.videowidth - context.settings.border); x += step)
        {
            if (c < static_cast<int>(comskip::detection::maximum_cutscene_pixels))
            {
                cs.push_back(context.state.frame_ptr[y * context.state.width + x]);
                ++c;
            }
        }
    }
//GetDumpFileName();
    if (context.settings.cutscenefile.c_str()[0] == 0)
    {
        context.settings.cutscenefile = std::string(context.state.workbasename) + ".dmp";
    }
    const auto path = comskip::platform::path_from_utf8(context.settings.cutscenefile);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_open, {context.settings.cutscenefile});
    if (!comskip::detection::write_cutscene(output,
            {static_cast<std::int32_t>(brightness), std::move(cs)}))
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_write, {context.settings.cutscenefile});
    output.close();
    if (!output)
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_write, {context.settings.cutscenefile});
    SceneDebug(context, 7, "scene_cutfile_saved", std::format("{:6}", frame_count),
               context.settings.cutscenefile);
}

void LoadCutScene(RecordingContext& context, std::string_view filename)
{
    const auto failed = [&] {
        Debug(context, 1, context.translator.format("detection_cutfile_read_failed", filename));
    };
    const int slot = context.state.cutscenes;
    if (slot < 0 || slot >= static_cast<int>(std::size(context.state.cutscene))) {
        failed();
        return;
    }
    std::ifstream input(comskip::platform::path_from_utf8(filename), std::ios::binary);
    if (!input) {
        Debug(context, 1, context.translator.format("detection_cutfile_open_failed", filename));
        return;
    }
    auto record = comskip::detection::read_cutscene(input);
    if (!record) {
        failed();
        return;
    }
    // Publish a complete record only after every file/size check succeeds.
    std::copy(record->pixels.begin(), record->pixels.end(), context.state.cutscene[slot].begin());
    context.state.csbrightness[slot] = record->brightness;
    context.state.cslength[slot] = static_cast<int>(record->pixels.size());
    ++context.state.cutscenes;
    SceneDebug(context, 7, "scene_cutfile_loaded", record->pixels.size(), filename);
}

void ScanBottom(RecordingContext& context, intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    static_cast<void>(arg);
    brightCount = 0;
    max_delta =  std::min(context.state.videowidth, context.state.height)/2 - context.settings.border;
    delta = 0;
    while (delta < max_delta)
    {
        y = context.settings.border + delta;
        x = context.settings.border + delta;
        i = y * context.state.width + x;
        i_max = y * context.state.width + context.state.videowidth - context.settings.border - delta;
        i_step = context.state.scan_step;
        for (; i < i_max; i += i_step)
        {
            if (context.state.haslogo[i])
                continue;
            hereBright = context.state.frame_ptr[i];
            context.state.own_histogram[0][BrightnessIndex(hereBright)]++;
            if (hereBright > context.settings.test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            //brightCountminY = 0;
            context.state.minY = y;
        }
        delta += context.state.scan_step;
    }
}

void ScanTop(RecordingContext& context, intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    static_cast<void>(arg);

    max_delta =  std::min(context.state.videowidth, context.state.height)/2 - context.settings.border;
    brightCount = 0;
    delta = 0;
    while (delta < max_delta)
    {
        x = context.settings.border + delta;
        y = std::min(context.state.height - 1, context.state.height - context.settings.border - delta);
        i = y * context.state.width + x;
        i_max = y * context.state.width + context.state.videowidth - context.settings.border - delta;
        i_step = context.state.scan_step;
        for (; i < i_max; i += i_step)
        {
            if (context.state.haslogo[i])
                continue;
            hereBright = context.state.frame_ptr[i];
            context.state.own_histogram[1][BrightnessIndex(hereBright)]++;
            if (hereBright > context.settings.test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            //brightCountmaxY = 0;
            context.state.maxY = y;
        }
        delta += context.state.scan_step;
    }
}

void ScanLeft(RecordingContext& context, intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    static_cast<void>(arg);

    max_delta =  std::min(context.state.videowidth, context.state.height)/2 - context.settings.border;
    brightCount = 0;
    delta = 0;
    while (delta < max_delta)
    {
        x = context.settings.border + delta;
        y = context.settings.border + delta;
        i = y * context.state.width + x;
        i_step = context.state.scan_step * context.state.width;
        i_max = (context.state.height - context.settings.border - delta) * context.state.width + x;
        for (; i< i_max; i += i_step)
        {
            if (context.state.haslogo[i])
                continue;
            hereBright = context.state.frame_ptr[i];
            context.state.own_histogram[2][BrightnessIndex(hereBright)]++;
            if (hereBright > context.settings.test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            //brightCountminX = 0;
            context.state.minX = x;
        }
        delta += context.state.scan_step;
    }
}

void ScanRight(RecordingContext& context, intptr_t arg)
{
    int		i, i_max, i_step;
    int		x;
    int		y;
    int     delta;
    int     max_delta;
    int		hereBright;
    int		brightCount;
    static_cast<void>(arg);

    max_delta =  std::min(context.state.videowidth, context.state.height)/2 - context.settings.border;
    brightCount = 0;
    delta = 0;
    while (delta < max_delta)
    {
        x = std::min(context.state.videowidth - 1, context.state.videowidth - context.settings.border - delta);
        y = context.settings.border + delta;
        i = y * context.state.width + x;
        i_step = context.state.scan_step * context.state.width;
        i_max = (context.state.height - context.settings.border - delta) * context.state.width + x;
        for (; i < i_max; i += i_step)
        {
            if (context.state.haslogo[i])
                continue;
            hereBright = context.state.frame_ptr[i];
            context.state.own_histogram[3][BrightnessIndex(hereBright)]++;
            if (hereBright > context.settings.test_brightness)
                brightCount++;
        }
        if (brightCount < 5)
        {
            context.state.maxX = x;
        }
        delta += context.state.scan_step;
    }
}

void DetectCredits(RecordingContext& context, int frame_count)
{




        if (frame_count > 1 &&
            abs(context.state.frame[frame_count].brightness - context.state.frame[frame_count-1]. brightness) < 2  &&
            context.state.frame[frame_count].brightness < context.settings.max_avg_brightness + 5
            ) {
                context.state.frame[frame_count].cutscenematch = context.state.frame[frame_count-1].cutscenematch - 1;
                context.state.DetectCredits_credit_length++;

        }
        else if ( context.state.DetectCredits_credit_length > context.settings.fps * 0.5)
        {
            context.state.frame[frame_count].cutscenematch = 100;
            if (abs(context.state.DetectCredits_credit_length - context.state.DetectCredits_prev_credit_length)< 10 &&
                  frame_count - context.state.DetectCredits_credit_length - context.state.DetectCredits_prev_credit_end < static_cast<int>(context.settings.fps)/2) {
                context.state.DetectCredits_credit_count++;
                if (context.state.DetectCredits_credit_count > 5)
                    SceneDebug(context, 1, "scene_credits_detected",
                               context.state.DetectCredits_credit_count, frame_count);
            }
            context.state.DetectCredits_prev_credit_end = frame_count;
            context.state.DetectCredits_prev_credit_length = context.state.DetectCredits_credit_length;
            context.state.DetectCredits_credit_length = 0;
        } else if (frame_count - context.state.DetectCredits_prev_credit_end  < static_cast<int>(context.settings.fps)/2) {
            context.state.frame[frame_count].cutscenematch = 100;
            context.state.DetectCredits_credit_length = 0;
        } else {
            context.state.frame[frame_count].cutscenematch = 100;
            context.state.DetectCredits_credit_length = 0;
            context.state.DetectCredits_credit_count = 0;
            context.state.DetectCredits_prev_credit_length = 0;
            context.state.DetectCredits_prev_credit_end = 0;
        }

}


bool CheckSceneHasChanged(RecordingContext& context)
{
    context.state.ensure_pixel_buffers(comskip::detection::method_enabled(
        context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo) || context.state.logoInfoAvailable);
    int		i;
    int		x;
    int		step;
    long	similar = 0;
//    static long prevsimilar = 0;
    int		hasBright = 0;
    int		dimCount = 0;
    bool	isDim = false;
    int pixels = 0;
//    int		brightCountminX;
//    int		brightCountminY;
//    int		brightCountmaxX;
//    int		brightCountmaxY;
    long	cause;
    int  uniform = 0;
    double scale = 1.0;

    const auto sampling = comskip::detection::validate_scene_sampling(
        context.state.videowidth, context.state.height, context.state.width, context.settings.border);
    comskip::detection::validate_scene_brightness(context.settings.max_brightness,
                                                context.settings.test_brightness);
    if (context.state.frame_ptr.empty() || context.state.haslogo.size() < sampling.storage_size)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::scene_sampling_requires_complete_image_and_logo_buffers);
    context.state.minY = context.settings.border;
    context.state.maxY = context.state.height - context.settings.border;
    context.state.minX = context.settings.border;
    context.state.maxX = context.state.videowidth - context.settings.border;
    step = sampling.step;
    context.state.scan_step = step;

    if (context.settings.edge_step == 0)
        context.settings.edge_step = step; // Automatic adjust edge step for video size

    std::ranges::copy(context.state.histogram, std::begin(context.state.lastHistogram));
    context.state.last_brightness = context.state.brightness;
    context.state.brightness = 0;

    // compare current frame with last frame here
//    memset(histogram, 0, sizeof(histogram));
    for (auto& row : context.state.own_histogram)
        std::ranges::fill(row, 0);

//    max_delta =  min(videowidth,height)/2 - border;

    if (context.settings.thread_count > 1) {

        if (!context.state.scan_workers) {
            context.state.scan_workers = std::make_unique<ScanWorkers>(std::array<std::function<void(intptr_t)>, 4>{
                [&context](intptr_t v) { ScanBottom(context, v); },
                [&context](intptr_t v) { ScanTop(context, v); },
                [&context](intptr_t v) { ScanLeft(context, v); },
                [&context](intptr_t v) { ScanRight(context, v); }});
        }
        context.state.scan_workers->run();
    } else {
        ScanBottom(context, static_cast<intptr_t>(0));
        ScanTop(context, static_cast<intptr_t>(0));
        ScanLeft(context, static_cast<intptr_t>(0));
        ScanRight(context, static_cast<intptr_t>(0));
    }
    for (i = 0; i < 256; i++) {
        context.state.histogram[i] = context.state.own_histogram[0][i] + context.state.own_histogram[1][i] + context.state.own_histogram[2][i] + context.state.own_histogram[3][i];
    }

#ifdef FRAME_WITH_HISTOGRAM
    if (framearray) std::ranges::copy(context.state.histogram,
                                      std::begin(context.state.frame[frame_count].histogram));
#endif
    if (context.state.framearray) context.state.frame[context.state.frame_count].minY = context.state.minY;
    if (context.state.framearray) context.state.frame[context.state.frame_count].maxY = context.state.maxY;
    if (context.state.framearray) context.state.frame[context.state.frame_count].minX = context.state.minX;
    if (context.state.framearray) context.state.frame[context.state.frame_count].maxX = context.state.maxX;

    if (context.state.framenum_real <= 1)
    {

        std::ranges::copy(context.state.histogram, std::begin(context.state.lastHistogram));
        context.state.last_brightness = context.state.brightness;


        if (comskip::detection::method_enabled(context.settings.commDetectMethod,
                                                comskip::detection::DetectionMethod::aspect_ratio))
        {
            ProcessARInfoInit(context, context.state.minY, context.state.maxY, context.state.minX, context.state.maxX);
            if (context.state.framearray) context.state.frame[context.state.frame_count].ar_ratio = context.state.last_ar_ratio;
        }

        ProcessACInfoInit(context, context.state.frame[context.state.frame_count].audio_channels);

        for (i = context.settings.max_brightness; i >= 0; i--) context.state.last_brightness += context.state.histogram[i] * i;
        context.state.last_brightness /= static_cast<long>(sampling.initial_brightness_divisor);
        if (context.state.framearray)
        {
            context.state.frame[context.state.frame_count].brightness = context.state.last_brightness;
            context.state.frame[context.state.frame_count].logo_present = false;
            context.state.frame[context.state.frame_count].schange_percent = 0;
        }

        if (comskip::detection::method_enabled(context.settings.commDetectMethod,
                                                comskip::detection::DetectionMethod::scene_change))
        {
            InitializeSchangeArray(context, 0);
            context.state.schange[0].frame = 0;
            context.state.schange[0].percentage = 0;
            context.state.schange_count++;
        }

        return (false);
    }
    if ( 17652 < context.state.frame_count && context.state.frame_count < 17657 )
    {
//		OutputFrame(frame_count);
    }

    ProcessARInfo(context, context.state.minY, context.state.maxY,context.state.minX, context.state.maxX);
    ProcessACInfo(context, context.state.frame[context.state.frame_count].audio_channels);
    if (context.state.framearray) context.state.frame[context.state.frame_count].ar_ratio = context.state.last_ar_ratio;

    similar *= 1;

    for (i = 255; i > context.settings.max_brightness; i--)
    {
        pixels += context.state.histogram[i];
        context.state.brightness += context.state.histogram[i] * i;
        if (context.state.histogram[i])
            hasBright++;
//		if (histogram[i] != lastHistogram[i]) similar += abs( histogram[i] - lastHistogram[i]);
        if (context.state.histogram[i] < context.state.lastHistogram[i]) similar += context.state.histogram[i];
        else similar += context.state.lastHistogram[i];
    }

    for (i = context.settings.max_brightness; i > context.settings.test_brightness; i--)
    {
        pixels += context.state.histogram[i];
        context.state.brightness += context.state.histogram[i] * i;
        dimCount += context.state.histogram[i];
//		if (histogram[i] != lastHistogram[i]) similar += abs( histogram[i] - lastHistogram[i]);
        if (context.state.histogram[i] < context.state.lastHistogram[i]) similar += context.state.histogram[i];
        else similar += context.state.lastHistogram[i];
    }

    for (i = context.settings.test_brightness; i >= 0; i--)
    {
        pixels += context.state.histogram[i];
        context.state.brightness += context.state.histogram[i] * i;
//		if (histogram[i] != lastHistogram[i]) similar += abs( histogram[i] - lastHistogram[i]);
        if (context.state.histogram[i] < context.state.lastHistogram[i]) similar += context.state.histogram[i];
        else similar += context.state.lastHistogram[i];
    }
    // A valid logo mask can exclude every sample. Keep the empty result zero.
    pixels = std::max(pixels, 1);
    context.state.brightness /= pixels;

    if (context.state.framearray) context.state.frame[context.state.frame_count].hasBright = hasBright;
    scale = 720.0 * 480.0 / context.state.width / context.state.height;
    if (context.state.min_hasBright > hasBright * scale)
        context.state.min_hasBright = hasBright * scale;

    if (context.state.framearray) context.state.frame[context.state.frame_count].dimCount = dimCount;
    if (context.state.min_dimCount > dimCount * scale)
        context.state.min_dimCount = dimCount * scale;

    if (context.settings.cutsceneno != 0 || context.state.frame_count == context.settings.cutsceneno)
        RecordCutScene(context, context.state.frame_count, context.state.brightness);



    uniform = 0;
    for (i = 255; i > context.state.brightness + context.settings.noise_level; i--)
    {
        uniform +=  context.state.histogram[i] * (i - context.state.brightness);
    }
    for (i = context.state.brightness - context.settings.noise_level; i >= 0; i--)
    {
        uniform +=  context.state.histogram[i] * (context.state.brightness - i);
    }
    uniform = static_cast<double>(uniform) * 730 / pixels;
    if (context.state.framearray) context.state.frame[context.state.frame_count].uniform = uniform;


    x = 0;
    for (i=10; i<100; i++)
    {
        if (context.state.histogram[i] > 10)
        {
            if (x<context.state.histogram[i])
                x=context.state.histogram[i];
            else
            {
                if (x > context.state.histogram[i+1] && context.state.histogram[i-1] > 1000)
                {
                    context.state.blackHistogram[i-1]++;
                    break;
                }
            }
        }
    }
    x = 0;
    for (i=10; i<100; i++)
    {
        if (context.state.blackHistogram[x] < context.state.blackHistogram[i])
        {
            x = i;
        }
    }
    /*	Not tested
        if (x > 10 && (frame_count % 2) == 0) {
            x = x + 5;
            if (x > max_avg_brightness) {
                max_avg_brightness++;
                test_brightness++;
                max_brightness++;
            } else if (x < max_avg_brightness) {
                max_avg_brightness--;
                test_brightness--;
                max_brightness--;
            }
        }
    */
    if (context.state.framearray) context.state.frame[context.state.frame_count].brightness = context.state.brightness;
    context.state.brightHistogram[std::clamp(context.state.brightness, 0, 255)]++;
    context.state.uniformHistogram[std::clamp(uniform / uniform_scale, 0, 255)]++;
    if ((dimCount > static_cast<int>(.05 * context.state.width * context.state.height)) &&
        (dimCount < static_cast<int>(.35 * context.state.width * context.state.height))) isDim = true;

    context.state.sceneChangePercent = static_cast<int>(100.0 * similar / pixels);
//	sceneChangePercent = (int)(100.0 * (1.0 - ((float)abs(prevsimilar - similar) / pixels)));
//    prevsimilar = similar;

    if (context.state.framearray) context.state.frame[context.state.frame_count].schange_percent = context.state.sceneChangePercent;


//	cause = ProcessClues(frame_count, brightness, hasBright, isDim, uniform, sceneChangePercent, curvolume,
//	if (framearray) frame[frame_count].isblack = cause;
//	if (cause != 0)
//		InsertBlackFrame(framenum_real,brightness,uniform,curvolume,cause;

    cause = 0;
    if (comskip::detection::method_enabled(context.settings.commDetectMethod,
                                            comskip::detection::DetectionMethod::black_frame))
    {
        if ((context.state.brightness <= context.settings.max_avg_brightness) &&
            hasBright <= comskip::detection::scaled_bright_pixel_limit(context.settings.maxbright,
                context.state.width,context.state.height) && !isDim)
        {
            cause |= cause_flag(comskip::detection::FrameCause::black);
            SceneDebug(context, 7, "scene_black_frame", std::format("{:6}", context.state.framenum_real),
                       std::format("{:.3f}", get_frame_pts(context, context.state.framenum_real)),
                       context.state.brightness, uniform,
                       context.state.black[std::max(0L, context.state.black_count - 1)].volume);
        }
        else if (context.settings.non_uniformity > 0)
        {
//????
            if ((context.state.brightness <= context.settings.max_avg_brightness) && uniform < context.settings.non_uniformity )
            {
                cause |= cause_flag(comskip::detection::FrameCause::non_uniform);
                SceneDebug(context, 7, "scene_black_frame", std::format("{:6}", context.state.framenum_real),
                           std::format("{:.3f}", get_frame_pts(context, context.state.framenum_real)),
                           context.state.brightness, uniform,
                           context.state.black[std::max(0L, context.state.black_count - 1)].volume);
            }
            if (context.state.brightness > context.settings.max_avg_brightness && uniform < context.settings.non_uniformity && context.state.brightness < 250 )
            {
                cause |= cause_flag(comskip::detection::FrameCause::non_uniform);
                SceneDebug(context, 7, "scene_uniform_frame", std::format("{:6}", context.state.framenum_real),
                           std::format("{:.3f}", get_frame_pts(context, context.state.framenum_real)),
                           context.state.brightness, uniform);
            }
        }
    }

 //   if (commDetectMethod & RESOLUTION_CHANGE)
    {
        if ((context.state.old_width != 0 && context.state.width != context.state.old_width) || (context.state.old_height != 0 && context.state.height != context.state.old_height))
        {
            cause |= cause_flag(comskip::detection::FrameCause::resolution_change);
            SceneDebug(context, 7, "scene_resolution_change", std::format("{:6}", context.state.framenum_real),
                       std::format("{:.3f}", get_frame_pts(context, context.state.framenum_real)),
                       context.state.old_width, context.state.old_height,
                       context.state.width, context.state.height);
            context.state.old_width = context.state.width;
            context.state.old_height = context.state.height;
            ResetLogoBuffers(context);
        }
        context.state.old_width = context.state.width;
        context.state.old_height = context.state.height;
    }


    /*
        if (abs(brightness - last_brightness) > brightness_jump) {
            cause |= C_s;
            Debug(7, "Frame %6i - Black frame because large brightness change from %i to %i with uniform %i\n", framenum_real, last_brightness, brightness, uniform);
        } // else
    */
    if (comskip::detection::method_enabled(context.settings.commDetectMethod,
                                            comskip::detection::DetectionMethod::scene_change))
    {

        if (!(context.state.frame[context.state.frame_count-1].isblack & cause_flag(comskip::detection::FrameCause::black)) && !(cause & cause_flag(comskip::detection::FrameCause::black)))
        {
            if (abs(context.state.frame[context.state.frame_count-1].brightness - context.state.last_brightness) > context.settings.brightness_jump)
            {
                SceneDebug(context, 7, "scene_large_brightness_change",
                           std::format("{:6}", context.state.framenum_real),
                           std::format("{:.3f}", get_frame_pts(context, context.state.framenum_real)),
                           context.state.last_brightness, context.state.brightness, uniform);
                cause |= cause_flag(comskip::detection::FrameCause::scene_change);
            }
            else if (/* sceneChangePercent */ context.state.frame[context.state.frame_count-1].schange_percent < context.state.schange_cutlevel)
            {
                SceneDebug(context, 7, "scene_large_scene_change",
                           std::format("{:6}", context.state.framenum_real),
                           std::format("{:.3f}", get_frame_pts(context, context.state.framenum_real)),
                           context.state.sceneChangePercent, uniform);

                cause |= cause_flag(comskip::detection::FrameCause::scene_change);
            }
        }

        /*
        if ((sceneChangePercent < 10) && (!hasBright) && !(cause & C_b)) {
        Debug(
        7,
        "Frame %6i - BlackFrame detected because of a nonbright scene change:\tsc - %i\tavg - %i\n",
        framenum_real,
        sceneChangePercent,
        brightness
        );
        cause |= C_s;
        } else if ((sceneChangePercent < 20) && (!hasBright) && !(cause & C_b)) {




                if (brightness < last_brightness * 2) {
                InitializeSchangeArray(schange_count);
                schange[schange_count].percentage = sceneChangePercent;
                schange[schange_count].frame = framenum_real;
                schange_count++;
                memcpy(lastHistogram, histogram, sizeof(histogram));
                //				Debug(7, "Frame %6i - Scene change with change percentage of %i\n", framenum_real, sceneChangePercent);
                return (true);
                }
                if (0) {
                Debug(
                7,
                "Frame %6i - BlackFrame detected because of scene change with brightness double:\tsc - %i\tavg - %i.................................................................\n",
                framenum_real,
                sceneChangePercent,
                brightness
                );
                cause |= cause_flag(comskip::detection::FrameCause::scene_change);
                }
        */

    }
    if (context.state.sceneChangePercent < context.state.schange_threshold)
    {
        // Scene Change threshold: original = 91
        InitializeSchangeArray(context, context.state.schange_count);
        context.state.schange[context.state.schange_count].percentage = context.state.sceneChangePercent;
        context.state.schange[context.state.schange_count].frame = context.state.framenum_real;
        context.state.schange_count++;
//	   memcpy(lastHistogram, histogram, sizeof(histogram));
        //			Debug(7, "Frame %6i (%.3fs) - Scene change with change percentage of %i\n", framenum_real, get_frame_pts(framenum_real), sceneChangePercent);
    }

//    for (i=0; i < 255; i++)               No used!!!!!!!!
//        if (histogram[i] > 10) break;

    if (context.state.brightness < context.state.min_brightness_found) context.state.min_brightness_found = context.state.brightness;

    if (context.state.framearray) context.state.frame[context.state.frame_count].cutscenematch = 100;
//	if (brightness > max_avg_brightness + 10)
    if (context.state.cutscenes)
    {

        if (context.state.framearray) context.state.frame[context.state.frame_count].cutscenematch = 100;
        for (i = 0; i < context.state.cutscenes; i++)
        {
            if (abs(context.state.brightness - context.state.csbrightness[i]) < 2)
            {
                context.state.cutscenematch = MatchCutScene(context, std::span{context.state.cutscene[i]});
                if (context.state.framearray)
                {
                    if (context.state.frame[context.state.frame_count].cutscenematch > context.state.cutscenematch*100/context.state.cslength[i])
                        context.state.frame[context.state.frame_count].cutscenematch = context.state.cutscenematch*100/context.state.cslength[i];
                    if (context.state.frame[context.state.frame_count].cutscenematch < context.settings.cutscenedelta)
                        cause |= cause_flag(comskip::detection::FrameCause::cutscene);
                }
            }
        }

    } else {
 //       DetectCredits(frame_count);
    }
//    if (frame[frame_count].cutscenematch < cutscenedelta)
//        cause |= C_t;

    if (comskip::detection::method_enabled(context.settings.commDetectMethod,
                                            comskip::detection::DetectionMethod::silence))
    {
        if (0 <= context.state.frame[context.state.frame_count].volume && context.state.frame[context.state.frame_count].volume < context.settings.max_silence && context.settings.min_silence == 1)
        {
            cause |= cause_flag(comskip::detection::FrameCause::silence);
        }
        if (0 == context.state.frame[context.state.frame_count].volume)
        {
            cause |= cause_flag(comskip::detection::FrameCause::silence);
        }
    }

    if (cause != 0)
        InsertBlackFrame(context, context.state.framenum_real,context.state.brightness,uniform,context.state.curvolume,cause);
    if (context.state.framearray) context.state.frame[context.state.frame_count].isblack = cause;

    return (false);
}



// Subroutines for Logo Detection
