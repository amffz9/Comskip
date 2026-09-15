#include "audio_samples.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace comskip::media {
namespace {
struct ResamplerDeleter {
    void operator()(SwrContext* context) const noexcept { swr_free(&context); }
};

void check(int result, const char* operation) {
    if (result >= 0) return;
    std::array<char, AV_ERROR_MAX_STRING_SIZE> message{};
    av_strerror(result, message.data(), message.size());
    throw std::runtime_error(std::string(operation) + ": " + message.data());
}
}

PlanarAudio normalize_audio(const AVFrame& frame) {
    const auto format = static_cast<AVSampleFormat>(frame.format);
    if (frame.sample_rate <= 0 || frame.nb_samples < 0 ||
        !av_channel_layout_check(&frame.ch_layout) || av_get_bytes_per_sample(format) == 0) {
        throw std::invalid_argument("Invalid decoded audio frame");
    }

    PlanarAudio result{frame.sample_rate,
        std::vector<std::vector<float>>(frame.ch_layout.nb_channels)};
    if (frame.nb_samples == 0) return result;

    const int input_planes = av_sample_fmt_is_planar(format) ? frame.ch_layout.nb_channels : 1;
    if (!frame.extended_data) throw std::invalid_argument("Missing decoded audio samples");
    std::vector<const std::uint8_t*> input;
    input.reserve(input_planes);
    for (int channel = 0; channel < input_planes; ++channel) {
        if (!frame.extended_data[channel]) throw std::invalid_argument("Missing decoded audio plane");
        input.push_back(frame.extended_data[channel]);
    }

    SwrContext* raw_context = nullptr;
    const int allocated = swr_alloc_set_opts2(&raw_context,
        &frame.ch_layout, AV_SAMPLE_FMT_FLTP, frame.sample_rate,
        &frame.ch_layout, format, frame.sample_rate, 0, nullptr);
    std::unique_ptr<SwrContext, ResamplerDeleter> context(raw_context);
    check(allocated, "Configure audio conversion");
    check(swr_init(context.get()), "Initialize audio conversion");
    const int capacity = swr_get_out_samples(context.get(), frame.nb_samples);
    check(capacity, "Size audio conversion");
    std::vector<std::uint8_t*> output;
    output.reserve(result.channels.size());
    for (auto& channel : result.channels) {
        channel.resize(capacity);
        output.push_back(reinterpret_cast<std::uint8_t*>(channel.data()));
    }
    const int count = swr_convert(context.get(), output.data(), capacity,
        input.data(), frame.nb_samples);
    check(count, "Convert audio samples");
    // With identical rates there is no resampling delay or pending sample tail.
    if (count != frame.nb_samples) throw std::runtime_error("Audio conversion changed sample count");
    for (auto& channel : result.channels) channel.resize(count);
    return result;
}
}
