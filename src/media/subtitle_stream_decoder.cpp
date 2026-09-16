#include "subtitle_stream_decoder.h"
#include "ffmpeg_resources.h"
#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>
extern "C" {
#include <libavcodec/codec_desc.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

namespace comskip::media {
namespace {
struct ParametersDeleter {
    void operator()(AVCodecParameters* value) const noexcept { avcodec_parameters_free(&value); }
};
using ParametersPtr = std::unique_ptr<AVCodecParameters, ParametersDeleter>;
struct SubtitleOwner {
    AVSubtitle value{};
    ~SubtitleOwner() { avsubtitle_free(&value); }
};
void checked(int status, const char* operation) {
    if (status >= 0) return;
    char error[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(status, error, sizeof(error));
    throw std::runtime_error(std::string(operation) + ": " + error);
}
std::int64_t add(std::int64_t left, std::int64_t right) {
    if (left < 0 || right < 0 || left > std::numeric_limits<std::int64_t>::max() - right)
        throw std::invalid_argument("Subtitle timestamp interval overflows");
    return left + right;
}
}
struct SubtitleStreamDecoder::Impl {
    ParametersPtr parameters;
    AVRational time_base;
    CodecPtr codec;
    std::string header;
    bool drained{};

    Impl(const AVCodecParameters& source, AVRational base) : time_base(base) {
        if (base.num <= 0 || base.den <= 0 || source.codec_type != AVMEDIA_TYPE_SUBTITLE ||
            source.extradata_size < 0 || (source.extradata_size > 0 && !source.extradata))
            throw std::invalid_argument("Invalid standalone subtitle parameters or time base");
        const auto* description = avcodec_descriptor_get(source.codec_id);
        if (description && (description->props & AV_CODEC_PROP_BITMAP_SUB))
            throw std::invalid_argument("Bitmap subtitle streams cannot produce SRT/SAMI text without OCR; select a text subtitle stream");
        if (!description || !(description->props & AV_CODEC_PROP_TEXT_SUB))
            throw std::invalid_argument("Unsupported standalone text subtitle codec");
        const auto* decoder = avcodec_find_decoder(source.codec_id);
        if (!decoder) throw std::runtime_error("FFmpeg standalone text subtitle decoder is unavailable");
        parameters.reset(avcodec_parameters_alloc());
        if (!parameters) throw std::bad_alloc{};
        checked(avcodec_parameters_copy(parameters.get(), &source), "Copying subtitle parameters failed");
        codec.reset(avcodec_alloc_context3(decoder));
        if (!codec) throw std::bad_alloc{};
        checked(avcodec_parameters_to_context(codec.get(), parameters.get()), "Copying subtitle codec parameters failed");
        codec->pkt_timebase = time_base;
        checked(avcodec_open2(codec.get(), decoder, nullptr), "Opening standalone subtitle decoder failed");
        if (codec->subtitle_header_size > 0)
            header.assign(reinterpret_cast<const char*>(codec->subtitle_header), codec->subtitle_header_size);
    }
    std::int64_t microseconds(std::int64_t ticks) const {
        if (ticks < 0) throw std::invalid_argument("Subtitle PTS and duration must be nonnegative");
        const auto result = av_rescale_q(ticks, time_base, AVRational{1, 1000000});
        if (result < 0) throw std::invalid_argument("Subtitle timestamp conversion overflows");
        return result;
    }
    std::vector<CaptionCue> packet(AVPacket& packet, bool flushing = false) {
        SubtitleOwner subtitle;
        int got = 0;
        checked(avcodec_decode_subtitle2(codec.get(), &subtitle.value, &got, &packet), "Decoding standalone subtitle packet failed");
        if (!got) return {};
        const auto origin = subtitle.value.pts != AV_NOPTS_VALUE ? subtitle.value.pts :
                            flushing ? 0 : microseconds(packet.pts);
        const auto start = add(origin, static_cast<std::int64_t>(subtitle.value.start_display_time) * 1000);
        auto end = add(origin, static_cast<std::int64_t>(subtitle.value.end_display_time) * 1000);
        // FFmpeg's subtitle wrapper expresses a packet's default duration in
        // integer milliseconds. Preserve the original finer stream precision
        // when the decoder did not supply a distinct display interval.
        if (!flushing && packet.duration > 0 && subtitle.value.start_display_time == 0 &&
            subtitle.value.end_display_time == av_rescale_q(packet.duration, time_base, AVRational{1, 1000}))
            end = add(origin, microseconds(packet.duration));
        if (end <= start && !flushing && packet.duration > 0) end = add(microseconds(packet.pts), microseconds(packet.duration));
        CaptionCue cue{CaptionTimestamp{start}, CaptionTimestamp{end}, {}};
        for (unsigned i = 0; i < subtitle.value.num_rects; ++i) {
            const auto* rect = subtitle.value.rects[i];
            if (rect->type == SUBTITLE_BITMAP)
                throw std::runtime_error("FFmpeg returned bitmap subtitle data; OCR is required for text output");
            CaptionRegion region{rect->text ? rect->text : "", rect->ass ? rect->ass : ""};
            if (!region.text.empty() || !region.ass.empty()) cue.regions.push_back(std::move(region));
        }
        if (cue.regions.empty()) return {};
        if (end <= start) throw std::invalid_argument("Standalone subtitle cue has no positive duration");
        return {std::move(cue)};
    }
};
SubtitleStreamDecoder::SubtitleStreamDecoder(const AVCodecParameters& parameters, AVRational base)
    : impl_(std::make_unique<Impl>(parameters, base)) {}
SubtitleStreamDecoder::~SubtitleStreamDecoder() = default;
SubtitleStreamDecoder::SubtitleStreamDecoder(SubtitleStreamDecoder&&) noexcept = default;
SubtitleStreamDecoder& SubtitleStreamDecoder::operator=(SubtitleStreamDecoder&&) noexcept = default;
std::vector<CaptionCue> SubtitleStreamDecoder::decode(std::span<const std::uint8_t> payload,
                                                     std::int64_t pts, std::int64_t duration) {
    if (impl_->drained) throw std::logic_error("Standalone subtitle decoder must be reset after EOF");
    if (payload.empty() || payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Empty or oversized standalone subtitle packet");
    const auto duration_us = impl_->microseconds(duration);
    add(impl_->microseconds(pts), duration_us);
    if (duration_us / 1000 > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("Subtitle duration exceeds FFmpeg's display interval range");
    auto packet = make_packet();
    checked(av_new_packet(packet.get(), static_cast<int>(payload.size())), "Allocating subtitle packet failed");
    std::copy(payload.begin(), payload.end(), packet->data);
    packet->pts = pts;
    packet->duration = duration;
    return impl_->packet(*packet);
}
std::vector<CaptionCue> SubtitleStreamDecoder::drain() {
    if (impl_->drained) return {};
    std::vector<CaptionCue> result;
    if (impl_->codec->codec->capabilities & AV_CODEC_CAP_DELAY) {
        for (unsigned attempt = 0; ; ++attempt) {
            if (attempt == 4096)
                throw std::runtime_error("Standalone subtitle decoder did not finish draining");
            auto packet = make_packet();
            auto pending = impl_->packet(*packet, true);
            if (pending.empty()) break;
            result.insert(result.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
        }
    }
    impl_->drained = true;
    return result;
}
void SubtitleStreamDecoder::reset() {
    impl_ = std::make_unique<Impl>(*impl_->parameters, impl_->time_base);
}
const std::string& SubtitleStreamDecoder::ass_header() const { return impl_->header; }
}
