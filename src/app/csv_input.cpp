#include "../localization/diagnostic.h"
#include "exit_requested.h"
#include "checked_format.h"
#include "comskip.h"
#include "debug.h"
#include "recording_context.h"
#include "csv_input.h"
#include "runtime.h"
#include "config/legacy_settings.h"
#include "detection/detection_methods.h"
#include "detection/captions.h"
#include "detection/detector_constants.h"
#include "detection/detector_runtime.h"
#include "detection/frame_causes.h"
#include "detection/storage.h"
#include "output/diagnostics.h"
#include "platform/platform.h"
#include "ui/review.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ranges>
#include <vector>
#include <stdexcept>
#include <limits>
#include <utility>
#include "input/file_stream.h"
#include "input/caption_packet.h"
#include "input/frame_record.h"
#include "input/reference_file.h"
#include "input/checked_number.h"
#include "detection/logo_sampling.h"

namespace {
using comskip::detection::FrameCause;
using comskip::detection::cause_value;

constexpr int aspect_ratio_cause = cause_value(FrameCause::aspect_ratio);
constexpr int non_uniform_cause = cause_value(FrameCause::non_uniform);
constexpr int black_cause = cause_value(FrameCause::black);
constexpr int scene_change_cause = cause_value(FrameCause::scene_change);
constexpr int silence_cause = cause_value(FrameCause::silence);
constexpr int cutscene_cause = cause_value(FrameCause::cutscene);
constexpr int resolution_change_cause = cause_value(FrameCause::resolution_change);

[[noreturn]] void throw_caption_packet_error(comskip::input::CaptionPacketError error) {
    using enum comskip::input::CaptionPacketError;
    using comskip::diagnostics::Code;
    switch (error) {
    case truncated_frame_header:
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(Code::truncated_persisted_caption_frame_header);
    case invalid_frame_header:
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(Code::invalid_persisted_caption_frame_header);
    case truncated_packet_length:
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(Code::truncated_persisted_caption_packet_length);
    case invalid_packet_length:
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(Code::invalid_persisted_caption_packet_length);
    case truncated_packet:
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(Code::truncated_persisted_caption_packet);
    case read_failure:
        throw comskip::diagnostics::DiagnosticError<std::runtime_error>(Code::cannot_read_input_file);
    }
    std::unreachable();
}
}

void PrintArgs(RecordingContext& context)
{
    for (std::size_t i = 0; i < context.state.argument.size(); ++i)
        std::cout << i << '\t' << context.state.argument[i] << '\n';
}

comskip::platform::FilePtr reopen_csv_inputs(RecordingContext& context)
{
    const auto csv_path = context.state.inbasename + ".csv";
    auto input = comskip::platform::open_file_owned(csv_path, "r");
    if (!input)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
            comskip::diagnostics::Code::missing_csv_input);

    const auto caption_path = context.state.inbasename + ".data";
    context.state.dump_data_file = comskip::platform::open_file_owned(caption_path, "rb");
    return input;
}




void ProcessCSV(RecordingContext& context, comskip::platform::FilePtr input)
{
    bool lastLogoTest = false, curLogoTest = false;
    int minminY = 10000, maxmaxY = 0, minminX = 10000, maxmaxX = 0;
    int cutscene_nonzero_count = 0, old_format = true, use_bright = 0;
    int i;
    if (!input) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::missing_csv_input);
    comskip::input::FileStreamBuffer buffer(*input);
    std::istream source(&buffer);
    source.exceptions(std::ios::badbit);
    auto header = comskip::input::read_text_line(source);
    if (!header) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_input_has_no_header);
    if (comskip::input::trim_ascii(*header) == "sep=," || comskip::input::trim_ascii(*header) == "sep=;") {
        header = comskip::input::read_text_line(source);
        if (!header) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_input_has_no_column_header);
    }
    const auto rate = comskip::input::parse_frame_rate(*header);
    const double frame_rate = rate.value_or(context.settings.fps);
    const auto logo_sample = comskip::detection::logo_sampling_interval(frame_rate, context.state.logoFreq);
    std::optional<double> previous_time;
    std::vector<comskip::input::FrameRecord> observations;
    while (const auto text = comskip::input::read_text_line(source)) {
        const auto record = comskip::input::parse_frame_record(*text);
        if (observations.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max() - 2))
            throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::csv_observation_count_exceeds_the_frame_index_range);
        if (record.number != static_cast<int>(observations.size() + 1))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_frame_numbers_must_be_consecutive_from_one);
        if (record.min_y < 0 || record.min_x < 0 || record.max_y < record.min_y || record.max_x < record.min_x ||
            record.max_y > max_height || record.max_x > max_width)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_observation_has_invalid_scan_bounds);
        const double timestamp = record.timestamp.value_or((record.number - 1) / frame_rate);
        if (!std::isfinite(timestamp) || timestamp > static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1e6 - 1 / frame_rate ||
            (previous_time && timestamp < *previous_time))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_timestamps_must_be_representable_and_monotonic);
        previous_time = timestamp;
        observations.push_back(record);
    }
    if (observations.empty()) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_input_has_no_observations);

    comskip::platform::FilePtr opened_caption_file;
    auto* caption_file = context.state.dump_data_file.get();
    if (!caption_file)
    {
        auto companion = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(context.state.inbasename.c_str())));
        companion += ".data";
        const auto name = companion.u8string();
        opened_caption_file = comskip::platform::open_file_owned(
            std::string_view(reinterpret_cast<const char*>(name.c_str()), name.size()), "rb");
        if (!opened_caption_file && context.state.inbasename != context.state.workbasename)
            opened_caption_file = comskip::platform::open_file_owned(context.state.workbasename + ".data", "rb");
        caption_file = opened_caption_file.get();
    }

    std::vector<comskip::input::PersistedCaptionPacket> caption_packets;
    if (caption_file) {
        comskip::input::FileStreamBuffer caption_buffer(*caption_file);
        std::istream caption_source(&caption_buffer);
        caption_source.exceptions(std::ios::badbit);
        while (true) {
            auto result = comskip::input::read_persisted_caption_packet(
                caption_source, sizeof(context.state.ccData));
            if (!result) throw_caption_packet_error(result.error());
            if (!*result) break;
            caption_packets.push_back(std::move(**result));
        }
    }
    if (opened_caption_file) context.state.dump_data_file = std::move(opened_caption_file);

    // Validate both input files completely before changing recording settings/state.
    if (rate) context.settings.fps = *rate;
    context.state.logoInfoAvailable = true;
    InitComSkip(context);
    context.state.frame_count = 1;
    context.state.pict_type = '?';
    for (const auto& record : observations) {
        InitializeFrameArray(context, record.number);
        auto& frame = context.state.frame[record.number];
        frame.brightness = record.brightness; frame.schange_percent = record.scene_change;
        frame.logo_present = record.logo; frame.uniform = record.uniform; frame.volume = record.volume;
        frame.minY = record.min_y; frame.maxY = record.max_y;
        frame.ar_ratio = record.aspect_ratio; frame.currentGoodEdge = record.good_edge;
        frame.isblack = record.black; frame.cutscenematch = record.cutscene_match;
        frame.minX = record.min_x; frame.maxX = record.max_x;
        frame.hasBright = record.bright_count; frame.dimCount = record.dim_count;
        frame.pts = record.timestamp.value_or((record.number - 1) / context.settings.fps);
        frame.cur_segment = record.segment; frame.audio_channels = record.audio_channels;
        frame.pict_type = '?';
        minminY = std::min(minminY, record.min_y); maxmaxY = std::max(maxmaxY, record.max_y);
        minminX = std::min(minminX, record.min_x); maxmaxX = std::max(maxmaxX, record.max_x);
        if (record.black != 0 && record.black != 1) old_format = false;
        if (record.cutscene_match > 0) ++cutscene_nonzero_count;
        ++context.state.frame_count;
    }
    observations.clear();
    context.state.frame[0].pts = context.state.frame[1].pts;
    context.state.frame[context.state.frame_count].pts = (context.state.frame_count - 1) / context.settings.fps;
    for (i=0; i < 1000 && i < context.state.frame_count; i++)
    {
        if (context.state.frame[i].hasBright > 0)
            use_bright = 1;
    }


    context.state.height = maxmaxY + minminY;
    context.state.videowidth = context.state.width = maxmaxX + minminX;

    context.state.last_brightness = context.state.frame[1].brightness;
    Debug(context, 8, context.translator.text("csv_loaded"));
    input.reset();

    context.state.black_count = 0;
    context.state.logo_block_count = 0;
    context.state.black_count = 0;
    context.state.schange_count = 0;
    context.state.min_brightness_found = 255;
    std::size_t caption_index = 0;
    for (i = 1; i < context.state.frame_count; i++)
    {
        context.state.framenum_real = i;
        while (caption_index < caption_packets.size() && caption_packets[caption_index].frame <= i)
        {
                const auto& caption_packet = caption_packets[caption_index];
                context.state.ccDataLen = static_cast<int>(caption_packet.payload.size());
                std::ranges::copy(caption_packet.payload, context.state.ccData.begin());
                context.state.framenum = caption_packet.frame;
#ifdef PROCESS_CC
                if (context.state.processCC) ProcessCCData(context);
                if (context.captions) context.captions->consume_stored_packet(
                    {context.state.ccData.data(), static_cast<std::size_t>(context.state.ccDataLen)},
                    std::chrono::duration_cast<comskip::media::CaptionTimestamp>(
                        std::chrono::duration<double>(context.state.frame[i].pts)));
#endif
                ++caption_index;
        }
        if (old_format)
        {
            if (context.state.frame[i].isblack)
            {
                if (context.state.frame[i].brightness <= 5)
                {
                    if (context.state.frame[i].brightness == 5)
                        context.state.frame[i].isblack = aspect_ratio_cause;
                    if (context.state.frame[i].brightness == 4)
                        context.state.frame[i].isblack = non_uniform_cause;         // Checked
                    if (context.state.frame[i].brightness == 3)
                        context.state.frame[i].isblack = scene_change_cause;
                    if (context.state.frame[i].brightness == 2)
                        context.state.frame[i].isblack = scene_change_cause;			// Checked
                    if (context.state.frame[i].brightness == 1)
                        context.state.frame[i].isblack = non_uniform_cause;
                    context.state.frame[i].brightness = context.settings.max_avg_brightness + 1;
                }
                else
                    context.state.frame[i].isblack = black_cause;
            }
            else
                context.state.frame[i].isblack = 0;
        }

        if (context.state.frame[i].brightness > 0)
        {
            context.state.brightHistogram[std::clamp(context.state.frame[i].brightness, 0, 255)]++;

            if (context.state.frame[i].brightness < context.state.min_brightness_found) context.state.min_brightness_found = context.state.frame[i].brightness;

            context.state.uniformHistogram[std::clamp(context.state.frame[i].uniform / comskip::detection::uniform_scale, 0, 255)]++;
        }

        if (context.state.frame[i].volume >= 0)
        {
            context.state.volumeHistogram[(context.state.frame[i].volume/context.state.volumeScale < 255 ? context.state.frame[i].volume/context.state.volumeScale : 255)]++;
            context.state.silenceHistogram[(context.state.frame[i].volume < 255 ? context.state.frame[i].volume : 255)]++;
        }


        if (context.state.frame[i].maxX == 0)
        {
            if (i == 1)
            {
                if (context.state.frame[i].ar_ratio < 0.5)
                {
                    if (context.state.frame[i].maxY + context.state.frame[i].minY < 600)
                        context.state.videowidth = context.state.width = 720;
                    else if (context.state.frame[i].maxY + context.state.frame[i].minY < 800)
                        context.state.videowidth = context.state.width = 1200;
                    else
                        context.state.videowidth = context.state.width = 1920;
                }
                else
                    context.state.videowidth = context.state.width = static_cast<int>(
                        (context.state.frame[i].maxY + context.state.frame[i].minY) * context.state.frame[i].ar_ratio);
            }
            context.state.frame[i].maxX = context.state.videowidth - 10;
            context.state.frame[i].minX = 10;
        }
        if (i == 1)
        {
            ProcessARInfoInit(context, context.state.frame[i].minY, context.state.frame[i].maxY, context.state.frame[i].minX, context.state.frame[i].maxX);
            ProcessACInfoInit(context, context.state.frame[i].audio_channels);
        }
        else
        {
            ProcessARInfo(context, context.state.frame[i].minY, context.state.frame[i].maxY, context.state.frame[i].minX, context.state.frame[i].maxX);
            ProcessACInfo(context, context.state.frame[i].audio_channels);
        }
        context.state.frame[i].ar_ratio = context.state.last_ar_ratio;


        if (!comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::resolution_change))
            context.state.frame[i].isblack &= ~resolution_change_cause;

        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::black_frame))
        {
            if ((context.state.frame[i].isblack & black_cause) && context.state.frame[i].brightness > context.settings.max_avg_brightness)
                context.state.frame[i].isblack &= ~black_cause;

            if (use_bright)
            {

                if (context.state.frame[i].hasBright > 0 && context.state.min_hasBright > context.state.frame[i].hasBright * 720 * 480 / context.state.videowidth / context.state.height) context.state.min_hasBright = context.state.frame[i].hasBright * 720 * 480 / context.state.videowidth / context.state.height;
                if (context.state.frame[i].dimCount > 0 && context.state.min_dimCount > context.state.frame[i].dimCount * 720 * 480 / context.state.videowidth / context.state.height) context.state.min_dimCount = context.state.frame[i].dimCount * 720 * 480 / context.state.videowidth / context.state.height;

                if (context.state.frame[i].brightness <= context.settings.max_avg_brightness && context.state.frame[i].hasBright < context.settings.maxbright &&
                    context.state.frame[i].dimCount < static_cast<int>(.05 * context.state.videowidth * context.state.height))
                    context.state.frame[i].isblack |= black_cause;
            }
            if (i>1) { // Uniform not calculated for frame 1
                context.state.frame[i].isblack &= ~non_uniform_cause;
                if (!(context.state.frame[i].isblack & black_cause) && context.settings.non_uniformity > 0 && context.state.frame[i].uniform < context.settings.non_uniformity && context.state.frame[i].brightness < 250)
                    context.state.frame[i].isblack |= non_uniform_cause;
            }
        }
        else
        {
            context.state.frame[i].isblack &= ~(non_uniform_cause | black_cause);
        }

        if (context.state.frame[i].isblack & scene_change_cause)
            context.state.frame[i].isblack &= ~scene_change_cause;


        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::scene_change) && !(context.state.frame[i-1].isblack & black_cause) && !(context.state.frame[i].isblack & black_cause))
        {
            if (context.state.frame[i].brightness > 5 && abs(context.state.frame[i].brightness - context.state.last_brightness) > context.settings.brightness_jump)
            {
                context.state.frame[i].isblack |= scene_change_cause;
            }
            if (context.state.frame[i].brightness > 5)
                context.state.last_brightness = context.state.frame[i].brightness;

            if (context.state.frame[i].brightness > 5 && context.state.frame[i].schange_percent < 15)
            {
                context.state.frame[i].isblack |= scene_change_cause;
            }
        }


        if (context.state.frame[i].isblack & cutscene_cause)
            context.state.frame[i].isblack &= ~cutscene_cause;

        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::cutscene) && cutscene_nonzero_count > 0)
        {
            if (context.state.frame[i].cutscenematch < context.settings.cutscenedelta)
                context.state.frame[i].isblack |= cutscene_cause;
        }

        if (context.state.frame[i].isblack & silence_cause)
            context.state.frame[i].isblack &= ~silence_cause;

        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::silence))
        {
            if (0 <= context.state.frame[i].volume && context.state.frame[i].volume < context.settings.max_silence && context.settings.min_silence == 1)
            {
                context.state.frame[i].isblack |= silence_cause;
            }
            if (context.state.frame[i].volume < 6)
            {
                context.state.frame[i].isblack |= silence_cause;
            }
        }


        if (context.state.frame[i].isblack)
        {
                    InsertBlackFrame(context, i, context.state.frame[i].brightness, context.state.frame[i].uniform,
                        context.state.frame[i].volume, static_cast<int>(context.state.frame[i].isblack));
        }

        if ((context.state.frame[i].schange_percent < 20) && i > 1 && context.state.black_count > 0 && (context.state.black[context.state.black_count - 1].frame != i))
        {
            if (context.state.frame[i].brightness < context.state.frame[i - 1].brightness * 2)
            {
                InitializeSchangeArray(context, context.state.schange_count);
                context.state.schange[context.state.schange_count].percentage = context.state.frame[i].schange_percent;
                context.state.schange[context.state.schange_count].frame = i;
                context.state.schange_count++;
            }
        }
        else if (context.state.frame[i].schange_percent < context.state.schange_threshold)
        {

            // Scene Change threshold: original = 91
            InitializeSchangeArray(context, context.state.schange_count);
            context.state.schange[context.state.schange_count].percentage = context.state.frame[i].schange_percent;
            context.state.schange[context.state.schange_count].frame = i;
            context.state.schange_count++;
        }

        if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo) && i % logo_sample == 0)
        {
            curLogoTest = (context.state.frame[i].currentGoodEdge > context.settings.logo_threshold);
            lastLogoTest = ProcessLogoTest(context, i, curLogoTest, false);
            context.state.frame[i].logo_present = lastLogoTest;
        }
        if (lastLogoTest) context.state.frames_with_logo++;
    }
    if (context.captions) {
        context.captions->finish(std::chrono::duration_cast<comskip::media::CaptionTimestamp>(
            std::chrono::duration<double>(context.state.frame[context.state.frame_count - 1].pts + 1 / context.settings.fps)));
        context.captions.reset();
    }
    // Parsing leaves count at the extra terminal slot. Detection and exports
    // use the decoder convention: count is the final real observation index.
    // Keep the allocated terminal slot, but exclude it from media statistics.
    --context.state.frame_count;
    context.state.framenum_real = context.state.frame_count;
    context.state.framesprocessed = context.state.frame_count;
    if (context.settings.output_live)
        BuildCommListAsYouGo(context);
    BuildMasterCommList(context);

    if (context.settings.output_debugwindow)
    {
#ifdef DEBUG
        skip_B_frames=0;
#endif
#ifdef DONATOR
        context.settings.skip_B_frames=0;
#endif
        context.state.processCC = 0;
        i = 0;
        std::cout << context.translator.text("csv_close_window");
        if (ReviewResult(context))
        {
            LoadIniFile(context);
            input = reopen_csv_inputs(context);
            return ProcessCSV(context, std::move(input));
        }
    }
    comskip::request_exit(0);
}

