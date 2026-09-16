#include "caption_decoder.h"
#include "ffmpeg_resources.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
extern "C" {
#include <libavutil/opt.h>
}

namespace comskip::media {
namespace {
struct SubtitleOwner {
    AVSubtitle value{};
    ~SubtitleOwner() { avsubtitle_free(&value); }
};
std::string_view ass_content(std::string_view ass) {
    // FFmpeg's ASS event has eight comma-separated metadata fields followed by
    // the display content. Keep the complete event for rendering/encoding.
    for (int field = 0; field < 8; ++field) {
        const auto comma = ass.find(',');
        if (comma == std::string_view::npos) return {};
        ass.remove_prefix(comma + 1);
    }
    return ass;
}
bool same_display(const std::vector<CaptionRegion>& left, const std::vector<CaptionRegion>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        // Only the first ASS field (read-order ID) changes for identical events.
        const auto without_order = [](const std::string& value) {
            const auto comma = value.find(',');
            return comma == std::string::npos ? std::string_view(value) : std::string_view(value).substr(comma + 1);
        };
        if (left[i].text != right[i].text || without_order(left[i].ass) != without_order(right[i].ass)) return false;
    }
    return true;
}
}
struct CaptionDecoder::Impl {
    CaptionField field;
    CodecPtr codec;
    std::string header;
    std::optional<CaptionTimestamp> last_time;
    std::optional<CaptionCue> display;
    bool drained{};

    explicit Impl(CaptionField selection) : field(selection) {
        const auto* decoder = avcodec_find_decoder(AV_CODEC_ID_EIA_608);
        if (!decoder) throw std::runtime_error("FFmpeg EIA-608 decoder is unavailable");
        codec.reset(avcodec_alloc_context3(decoder));
        if (!codec) throw std::bad_alloc{};
        codec->pkt_timebase = {1, 1000000};
        if (av_opt_set_int(codec->priv_data, "real_time", 1, 0) < 0 ||
            av_opt_set_int(codec->priv_data, "real_time_latency_msec", 0, 0) < 0 ||
            av_opt_set_int(codec->priv_data, "data_field", field == CaptionField::first ? 0 : 1, 0) < 0 ||
            avcodec_open2(codec.get(), decoder, nullptr) < 0)
            throw std::runtime_error("Cannot initialize FFmpeg EIA-608 decoder");
        if (codec->subtitle_header_size > 0)
            header.assign(reinterpret_cast<const char*>(codec->subtitle_header), codec->subtitle_header_size);
    }
    void validate_time(CaptionTimestamp timestamp) const {
        if (timestamp.count() < 0 || (last_time && timestamp < *last_time))
            throw std::invalid_argument("Caption timestamps must be nonnegative and monotonic");
    }
    void close_display(CaptionTimestamp end, std::vector<CaptionCue>& result) {
        if (display && end > display->start) {
            display->end = end;
            result.push_back(std::move(*display));
        }
        display.reset();
    }
    std::vector<CaptionCue> packet(std::span<const std::uint8_t> bytes, CaptionTimestamp timestamp) {
        auto packet = make_packet();
        if (!bytes.empty()) {
            if (av_new_packet(packet.get(), static_cast<int>(bytes.size())) < 0) throw std::bad_alloc{};
            std::copy(bytes.begin(), bytes.end(), packet->data);
        }
        packet->pts = timestamp.count();
        SubtitleOwner subtitle;
        int got_subtitle = 0;
        if (avcodec_decode_subtitle2(codec.get(), &subtitle.value, &got_subtitle, packet.get()) < 0)
            throw std::runtime_error("FFmpeg EIA-608 caption decoding failed");
        std::vector<CaptionCue> result;
        if (got_subtitle) {
            std::vector<CaptionRegion> screen;
            // A single packet can emit several display snapshots at its one
            // timestamp; the last snapshot is the visible display for this frame.
            for (unsigned i = 0; i < subtitle.value.num_rects; ++i) {
                const auto* rect = subtitle.value.rects[i];
                CaptionRegion region{rect->text ? rect->text : "", rect->ass ? rect->ass : ""};
                screen.clear();
                if (!region.text.empty() || !ass_content(region.ass).empty()) screen.push_back(std::move(region));
            }
            if (!display || !same_display(display->regions, screen)) {
                close_display(timestamp, result);
                if (!screen.empty()) display = CaptionCue{timestamp, {}, std::move(screen)};
            }
        }
        return result;
    }
};
CaptionDecoder::CaptionDecoder(CaptionField field) : impl_(std::make_unique<Impl>(field)) {}
CaptionDecoder::~CaptionDecoder() = default;
CaptionDecoder::CaptionDecoder(CaptionDecoder&&) noexcept = default;
CaptionDecoder& CaptionDecoder::operator=(CaptionDecoder&&) noexcept = default;
std::vector<CaptionCue> CaptionDecoder::decode(std::span<const std::uint8_t> a53, CaptionTimestamp timestamp) {
    if (impl_->drained) throw std::logic_error("Caption decoder must be reset after EOF");
    if (a53.size() % 3 != 0 || a53.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Malformed or oversized A53 caption packet");
    impl_->validate_time(timestamp);
    auto result = impl_->packet(a53, timestamp);
    impl_->last_time = timestamp;
    return result;
}
std::vector<CaptionCue> CaptionDecoder::drain(CaptionTimestamp end) {
    impl_->validate_time(end);
    if (impl_->drained) return {};
    auto result = impl_->packet({}, end);
    impl_->close_display(end, result);
    impl_->last_time = end;
    impl_->drained = true;
    return result;
}
void CaptionDecoder::reset() { impl_ = std::make_unique<Impl>(impl_->field); }
const std::string& CaptionDecoder::ass_header() const { return impl_->header; }
}
