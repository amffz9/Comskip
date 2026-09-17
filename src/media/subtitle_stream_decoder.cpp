#include "../localization/diagnostic.h"
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
using ParametersPtr = CodecParametersPtr;
void checked(int status, comskip::diagnostics::Code operation) {
    if (status >= 0) return;
    char error[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(status, error, sizeof(error));
    throw comskip::diagnostics::DiagnosticError<std::runtime_error>(operation, {error});
}
std::int64_t add(std::int64_t left, std::int64_t right) {
    if (left < 0 || right < 0 || left > std::numeric_limits<std::int64_t>::max() - right)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::subtitle_timestamp_interval_overflows);
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
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_standalone_subtitle_parameters_or_time_base);
        const auto* description = avcodec_descriptor_get(source.codec_id);
        if (description && (description->props & AV_CODEC_PROP_BITMAP_SUB))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::bitmap_subtitle_streams_cannot_produce_srt_sami_text_without_ocr_select_a_text_subtitle_stream);
        if (!description || !(description->props & AV_CODEC_PROP_TEXT_SUB))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::unsupported_standalone_text_subtitle_codec);
        const auto* decoder = avcodec_find_decoder(source.codec_id);
        if (!decoder) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::ffmpeg_standalone_text_subtitle_decoder_is_unavailable);
        parameters.reset(avcodec_parameters_alloc());
        if (!parameters) throw std::bad_alloc{};
        checked(avcodec_parameters_copy(parameters.get(), &source), comskip::diagnostics::Code::copying_subtitle_parameters_failed_detail);
        codec.reset(avcodec_alloc_context3(decoder));
        if (!codec) throw std::bad_alloc{};
        checked(avcodec_parameters_to_context(codec.get(), parameters.get()), comskip::diagnostics::Code::copying_subtitle_codec_parameters_failed_detail);
        codec->pkt_timebase = time_base;
        checked(avcodec_open2(codec.get(), decoder, nullptr), comskip::diagnostics::Code::opening_standalone_subtitle_decoder_failed_detail);
        if (codec->subtitle_header_size > 0)
            header.assign(reinterpret_cast<const char*>(codec->subtitle_header), codec->subtitle_header_size);
    }
    std::int64_t microseconds(std::int64_t ticks) const {
        if (ticks < 0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::subtitle_pts_and_duration_must_be_nonnegative);
        const auto result = av_rescale_q(ticks, time_base, AVRational{1, 1000000});
        if (result < 0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::subtitle_timestamp_conversion_overflows);
        return result;
    }
    std::vector<CaptionCue> packet(AVPacket& packet, bool flushing = false) {
        SubtitleOwner subtitle;
        int got = 0;
        checked(avcodec_decode_subtitle2(codec.get(), &subtitle.value, &got, &packet), comskip::diagnostics::Code::decoding_standalone_subtitle_packet_failed_detail);
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
                throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::ffmpeg_returned_bitmap_subtitle_data_ocr_is_required_for_text_output);
            CaptionRegion region{rect->text ? rect->text : "", rect->ass ? rect->ass : ""};
            if (!region.text.empty() || !region.ass.empty()) cue.regions.push_back(std::move(region));
        }
        if (cue.regions.empty()) return {};
        if (end <= start) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::standalone_subtitle_cue_has_no_positive_duration);
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
    if (impl_->drained) throw comskip::diagnostics::DiagnosticError<std::logic_error>(comskip::diagnostics::Code::standalone_subtitle_decoder_must_be_reset_after_eof);
    if (payload.empty() || payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::empty_or_oversized_standalone_subtitle_packet);
    const auto duration_us = impl_->microseconds(duration);
    add(impl_->microseconds(pts), duration_us);
    if (duration_us / 1000 > std::numeric_limits<std::uint32_t>::max())
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::subtitle_duration_exceeds_ffmpeg_s_display_interval_range);
    auto packet = make_packet();
    checked(av_new_packet(packet.get(), static_cast<int>(payload.size())), comskip::diagnostics::Code::allocating_subtitle_packet_failed_detail);
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
                throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::standalone_subtitle_decoder_did_not_finish_draining);
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
