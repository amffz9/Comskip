#include "../localization/diagnostic.h"
#include "audio_samples.h"
#include "ffmpeg_resources.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace comskip::media {
namespace {
void check(int result, comskip::diagnostics::Code operation) {
    if (result >= 0) return;
    std::array<char, AV_ERROR_MAX_STRING_SIZE> message{};
    av_strerror(result, message.data(), message.size());
    throw comskip::diagnostics::DiagnosticError<std::runtime_error>(operation, {message.data()});
}
}

PlanarAudio normalize_audio(const AVFrame& frame) {
    const auto format = static_cast<AVSampleFormat>(frame.format);
    if (frame.sample_rate <= 0 || frame.nb_samples < 0 ||
        !av_channel_layout_check(&frame.ch_layout) || av_get_bytes_per_sample(format) == 0) {
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_decoded_audio_frame);
    }

    PlanarAudio result{frame.sample_rate,
        std::vector<std::vector<float>>(frame.ch_layout.nb_channels)};
    if (frame.nb_samples == 0) return result;

    const int input_planes = av_sample_fmt_is_planar(format) ? frame.ch_layout.nb_channels : 1;
    if (!frame.extended_data) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::missing_decoded_audio_samples);
    std::vector<const std::uint8_t*> input;
    input.reserve(input_planes);
    for (int channel = 0; channel < input_planes; ++channel) {
        if (!frame.extended_data[channel]) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::missing_decoded_audio_plane);
        input.push_back(frame.extended_data[channel]);
    }

    SwrContext* raw_context = nullptr;
    const int allocated = swr_alloc_set_opts2(&raw_context,
        &frame.ch_layout, AV_SAMPLE_FMT_FLTP, frame.sample_rate,
        &frame.ch_layout, format, frame.sample_rate, 0, nullptr);
    ResamplerPtr context(raw_context);
    check(allocated, comskip::diagnostics::Code::configure_audio_conversion_detail);
    check(swr_init(context.get()), comskip::diagnostics::Code::initialize_audio_conversion_detail);
    const int capacity = swr_get_out_samples(context.get(), frame.nb_samples);
    check(capacity, comskip::diagnostics::Code::size_audio_conversion_detail);
    std::vector<std::uint8_t*> output;
    output.reserve(result.channels.size());
    for (auto& channel : result.channels) {
        channel.resize(capacity);
        output.push_back(reinterpret_cast<std::uint8_t*>(channel.data()));
    }
    const int count = swr_convert(context.get(), output.data(), capacity,
        input.data(), frame.nb_samples);
    check(count, comskip::diagnostics::Code::convert_audio_samples_detail);
    // With identical rates there is no resampling delay or pending sample tail.
    if (count != frame.nb_samples) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::audio_conversion_changed_sample_count);
    for (auto& channel : result.channels) channel.resize(count);
    return result;
}
}
