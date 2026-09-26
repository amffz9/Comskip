#include "audio_samples.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
struct FrameDeleter {
    void operator()(AVFrame* frame) const noexcept { av_frame_free(&frame); }
};
using Frame = std::unique_ptr<AVFrame, FrameDeleter>;
constexpr std::array<float, 4> values{-1.0f, -0.5f, 0.0f, 0.5f};

template<class T>
void store(std::uint8_t* destination, T value) {
    std::memcpy(destination, &value, sizeof(value));
}

void store_sample(std::uint8_t* destination, AVSampleFormat format, float value) {
    switch (av_get_packed_sample_fmt(format)) {
    case AV_SAMPLE_FMT_U8: store(destination, static_cast<std::uint8_t>(value * 128 + 128)); break;
    case AV_SAMPLE_FMT_S16: store(destination, static_cast<std::int16_t>(value * 32768)); break;
    case AV_SAMPLE_FMT_S32: store(destination, static_cast<std::int32_t>(static_cast<double>(value) * 2147483648.0)); break;
    case AV_SAMPLE_FMT_FLT: store(destination, value); break;
    case AV_SAMPLE_FMT_DBL: store(destination, static_cast<double>(value)); break;
    default: throw std::invalid_argument("Unsupported test format");
    }
}

Frame make_frame(AVSampleFormat format, int channels = 2) {
    Frame frame(av_frame_alloc());
    if (!frame) throw std::bad_alloc();
    frame->format = format;
    frame->sample_rate = 48000;
    frame->nb_samples = values.size();
    if (channels == 24) {
        const AVChannelLayout layout = AV_CHANNEL_LAYOUT_22POINT2;
        if (av_channel_layout_copy(&frame->ch_layout, &layout) < 0) throw std::bad_alloc();
    } else {
        av_channel_layout_default(&frame->ch_layout, channels);
    }
    if (av_frame_get_buffer(frame.get(), 0) < 0) throw std::bad_alloc();
    const bool planar = av_sample_fmt_is_planar(format);
    const int bytes = av_get_bytes_per_sample(format);
    for (int channel = 0; channel < channels; ++channel) {
        for (int sample = 0; sample < frame->nb_samples; ++sample) {
            const int plane = planar ? channel : 0;
            const int index = planar ? sample : sample * channels + channel;
            store_sample(frame->extended_data[plane] + index * bytes,
                format, values[(sample + channel) % values.size()]);
        }
    }
    return frame;
}

void expect_preserved(const AVFrame& frame) {
    const auto audio = comskip::media::normalize_audio(frame);
    EXPECT_EQ(audio.sample_rate, frame.sample_rate);
    ASSERT_EQ(audio.channels.size(), frame.ch_layout.nb_channels);
    for (std::size_t channel = 0; channel < audio.channels.size(); ++channel) {
        ASSERT_EQ(audio.channels[channel].size(), frame.nb_samples);
        for (std::size_t sample = 0; sample < audio.channels[channel].size(); ++sample) {
            EXPECT_FLOAT_EQ(audio.channels[channel][sample], values[(sample + channel) % values.size()]);
        }
    }
}

class AudioSamples : public testing::TestWithParam<AVSampleFormat> {};

TEST_P(AudioSamples, PreservesSamplesRateAndChannelOrder) {
    const auto frame = make_frame(GetParam());
    expect_preserved(*frame);
}

INSTANTIATE_TEST_SUITE_P(Formats, AudioSamples, testing::Values(
    AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_FLTP,
    AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_S32P, AV_SAMPLE_FMT_U8, AV_SAMPLE_FMT_U8P,
    AV_SAMPLE_FMT_DBL, AV_SAMPLE_FMT_DBLP));

TEST(AudioSamplesExtendedData, PreservesTwentyFourPlanarChannels) {
    const auto frame = make_frame(AV_SAMPLE_FMT_S16P, 24);
    expect_preserved(*frame);
}

TEST(AudioSamplesValidation, RejectsMissingPlaneAndInvalidSampleRate) {
    auto frame = make_frame(AV_SAMPLE_FMT_S16P);
    frame->sample_rate = 0;
    EXPECT_THROW(comskip::media::normalize_audio(*frame), std::invalid_argument);
    frame->sample_rate = 48000;
    auto* saved_plane = frame->extended_data[1];
    frame->extended_data[1] = nullptr;
    EXPECT_THROW(comskip::media::normalize_audio(*frame), std::invalid_argument);
    frame->extended_data[1] = saved_plane;
}
TEST(AudioSamples, ReusedNormalizerMatchesOneShotConversionAcrossFormatChanges) {
    comskip::media::AudioNormalizer normalizer;
    for (const auto& frame : {make_frame(AV_SAMPLE_FMT_S16), make_frame(AV_SAMPLE_FMT_S16),
                              make_frame(AV_SAMPLE_FMT_FLTP, 6), make_frame(AV_SAMPLE_FMT_S16)}) {
        const auto reused = normalizer.normalize(*frame);
        const auto one_shot = comskip::media::normalize_audio(*frame);
        EXPECT_EQ(reused.sample_rate, one_shot.sample_rate);
        EXPECT_EQ(reused.channels, one_shot.channels);
    }
}
}
