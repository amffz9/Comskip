#include "media/subtitle_output.h"
#include "media/ffmpeg_resources.h"
#include <pugixml.hpp>
#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace comskip::media {
namespace {
struct OutputDeleter {
    void operator()(AVFormatContext* value) const noexcept {
        if (value) { if (value->pb) avio_closep(&value->pb); avformat_free_context(value); }
    }
};
using OutputPtr = std::unique_ptr<AVFormatContext, OutputDeleter>;
std::string utf8(const std::filesystem::path& path) {
    auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
void check(int status, const char* operation) {
    if (status < 0) {
        char detail[AV_ERROR_MAX_STRING_SIZE]; av_strerror(status, detail, sizeof(detail));
        throw std::runtime_error(std::string(operation) + ": " + detail);
    }
}
pugi::xml_node child(pugi::xml_node parent, const char* name) {
    auto node = parent.append_child(name); if (!node) throw std::bad_alloc{}; return node;
}
void attribute(pugi::xml_node node, const char* name, const std::string& value) {
    if (!node.append_attribute(name).set_value(value.c_str())) throw std::bad_alloc{};
}
std::string xml_text(std::string_view input) {
    pugi::xml_document document;
    std::string value(input);
    if (!document.append_child(pugi::node_pcdata).set_value(value.c_str())) throw std::bad_alloc{};
    std::ostringstream output;
    document.save(output, "", pugi::format_raw | pugi::format_no_declaration, pugi::encoding_utf8);
    return output.str();
}
void sami_text(pugi::xml_node parent, int& alignment) {
    for (auto node = parent.first_child(); node;) {
        auto next = node.next_sibling();
        if (node.type() == pugi::node_pcdata) {
            std::string normalized = node.value();
            // SUBRIP's encoder emits this positioning token alongside HTML
            // styles. SAMI expresses horizontal alignment on the paragraph.
            auto position = normalized.find("{\\an");
            if (position != std::string::npos && normalized.size() >= position + 6 &&
                normalized[position + 4] >= '1' && normalized[position + 4] <= '9' && normalized[position + 5] == '}') {
                alignment = normalized[position + 4] - '0';
                normalized.erase(position, 6);
                if (!node.set_value(normalized.c_str())) throw std::bad_alloc{};
            }
            std::string_view value = normalized;
            if (value.contains('\n')) {
                while (true) {
                    auto newline = value.find('\n');
                    std::string part(value.substr(0, newline));
                    if (!parent.insert_child_before(pugi::node_pcdata, node).set_value(part.c_str())) throw std::bad_alloc{};
                    if (newline == std::string_view::npos) break;
                    if (!parent.insert_child_before("BR", node)) throw std::bad_alloc{};
                    value.remove_prefix(newline + 1);
                }
                parent.remove_child(node);
            }
        } else sami_text(node, alignment);
        node = next;
    }
}
}
struct SubtitleOutput::Impl {
    SubtitleFormat format;
    std::string header;
    CodecPtr encoder;
    OutputPtr muxer;
    AVStream* stream{};
    pugi::xml_document sami;
    pugi::xml_node body;
    std::ofstream file;
    std::optional<CaptionTimestamp> last_end;
    bool finished{};

    Impl(const std::filesystem::path& destination, SubtitleFormat selection, std::string_view ass_header)
        : format(selection), header(ass_header) {
        if (header.empty() || header.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) - AV_INPUT_BUFFER_PADDING_SIZE)
            throw std::invalid_argument("Subtitle output requires an ASS header");
        const auto* codec = avcodec_find_encoder(AV_CODEC_ID_SUBRIP);
        if (!codec) throw std::runtime_error("FFmpeg SubRip encoder is unavailable");
        encoder.reset(avcodec_alloc_context3(codec)); if (!encoder) throw std::bad_alloc{};
        encoder->time_base = {1, 1000000};
        encoder->subtitle_header = static_cast<std::uint8_t*>(av_mallocz(header.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!encoder->subtitle_header) throw std::bad_alloc{};
        std::copy(header.begin(), header.end(), encoder->subtitle_header);
        encoder->subtitle_header_size = static_cast<int>(header.size());
        check(avcodec_open2(encoder.get(), codec, nullptr), "Opening subtitle encoder");
        if (format == SubtitleFormat::srt) {
            AVFormatContext* output = nullptr;
            const auto filename = utf8(destination);
            const int allocated = avformat_alloc_output_context2(&output, nullptr, "srt", filename.c_str());
            muxer.reset(output);
            check(allocated, "Creating subtitle muxer"); if (!muxer) throw std::bad_alloc{};
            stream = avformat_new_stream(muxer.get(), nullptr); if (!stream) throw std::bad_alloc{};
            stream->time_base = {1, 1000000};
            check(avcodec_parameters_from_context(stream->codecpar, encoder.get()), "Configuring subtitle stream");
            check(avio_open(&muxer->pb, filename.c_str(), AVIO_FLAG_WRITE), "Opening subtitle destination");
            check(avformat_write_header(muxer.get(), nullptr), "Writing subtitle header");
        } else {
            auto declaration = sami.append_child(pugi::node_declaration);
            attribute(declaration, "version", "1.0"); attribute(declaration, "encoding", "UTF-8");
            auto root = child(sami, "SAMI"); child(root, "HEAD"); body = child(root, "BODY");
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.open(destination, std::ios::binary | std::ios::trunc);
        }
    }
    ~Impl() { try { finish(); } catch (...) {} }
    std::string encode(const CaptionCue& cue, bool escape_xml) {
        std::vector<AVSubtitleRect> rects(cue.regions.size());
        std::vector<AVSubtitleRect*> pointers; pointers.reserve(rects.size());
        std::vector<std::string> values; values.reserve(rects.size());
        for (std::size_t i = 0; i < rects.size(); ++i) {
            if (cue.regions[i].ass.empty()) throw std::invalid_argument("Subtitle region requires ASS data");
            values.push_back(escape_xml ? xml_text(cue.regions[i].ass) : cue.regions[i].ass);
            rects[i].type = SUBTITLE_ASS; rects[i].ass = values.back().data(); pointers.push_back(&rects[i]);
        }
        AVSubtitle subtitle{};
        subtitle.pts = cue.start.count(); subtitle.format = 1;
        subtitle.num_rects = static_cast<unsigned>(rects.size()); subtitle.rects = pointers.data();
        std::vector<std::uint8_t> buffer(4096);
        while (true) {
            const int size = avcodec_encode_subtitle(encoder.get(), buffer.data(), static_cast<int>(buffer.size()), &subtitle);
            if (size == AVERROR_BUFFER_TOO_SMALL) {
                if (buffer.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) / 2)
                    throw std::length_error("Subtitle event is too large");
                buffer.resize(buffer.size() * 2); continue;
            }
            check(size, "Encoding subtitle event");
            return {reinterpret_cast<const char*>(buffer.data()), static_cast<std::size_t>(size)};
        }
    }
    void write(const CaptionCue& cue) {
        if (finished) throw std::logic_error("Subtitle output must be reset after completion");
        if (cue.start.count() < 0 || cue.end <= cue.start || (last_end && cue.start < *last_end))
            throw std::invalid_argument("Subtitle cues must be nonnegative, ordered and nonoverlapping");
        const auto text = encode(cue, format == SubtitleFormat::sami);
        if (!text.empty()) {
            if (format == SubtitleFormat::srt) {
                auto packet = make_packet();
                check(av_new_packet(packet.get(), static_cast<int>(text.size())), "Allocating subtitle packet");
                std::copy(text.begin(), text.end(), packet->data);
                packet->stream_index = stream->index;
                packet->pts = packet->dts = av_rescale_q(cue.start.count(), AVRational{1, 1000000}, stream->time_base);
                packet->duration = av_rescale_q(cue.end.count(), AVRational{1, 1000000}, stream->time_base) - packet->pts;
                check(av_interleaved_write_frame(muxer.get(), packet.get()), "Writing subtitle packet");
                avio_flush(muxer->pb); check(muxer->pb->error, "Flushing subtitle packet");
            } else {
                pugi::xml_document fragment;
                const auto source = "<P>" + text + "</P>";
                if (!fragment.load_string(source.c_str(), pugi::parse_default | pugi::parse_ws_pcdata))
                    throw std::runtime_error("FFmpeg subtitle markup cannot be represented as SAMI");
                auto sync = child(body, "SYNC");
                attribute(sync, "Start", std::to_string(av_rescale_q(cue.start.count(), AVRational{1, 1000000}, AVRational{1, 1000})));
                auto paragraph = sync.append_copy(fragment.child("P")); if (!paragraph) throw std::bad_alloc{};
                attribute(paragraph, "Class", "UNKNOWNCC");
                int alignment = 2; sami_text(paragraph, alignment);
                attribute(paragraph, "style", alignment % 3 == 1 ? "text-align:left" : alignment % 3 == 0 ? "text-align:right" : "text-align:center");
                auto clear = child(body, "SYNC");
                attribute(clear, "Start", std::to_string(av_rescale_q(cue.end.count(), AVRational{1, 1000000}, AVRational{1, 1000})));
                auto blank = child(clear, "P"); attribute(blank, "Class", "UNKNOWNCC");
                if (!blank.text().set("\xc2\xa0")) throw std::bad_alloc{};
            }
        }
        last_end = cue.end;
    }
    void finish() {
        if (finished) return;
        if (format == SubtitleFormat::srt) {
            check(av_write_trailer(muxer.get()), "Completing subtitle file");
            avio_flush(muxer->pb); check(muxer->pb->error, "Flushing subtitle file");
            const int closed = avio_closep(&muxer->pb);
            finished = true;
            check(closed, "Closing subtitle file");
        } else {
            sami.save(file, "", pugi::format_raw, pugi::encoding_utf8);
            file.close();
        }
        finished = true;
    }
};
SubtitleOutput::SubtitleOutput(const std::filesystem::path& destination, SubtitleFormat format, std::string_view header)
    : impl_(std::make_unique<Impl>(destination, format, header)) {}
SubtitleOutput::~SubtitleOutput() = default;
SubtitleOutput::SubtitleOutput(SubtitleOutput&&) noexcept = default;
SubtitleOutput& SubtitleOutput::operator=(SubtitleOutput&&) noexcept = default;
void SubtitleOutput::write(const CaptionCue& cue) { impl_->write(cue); }
void SubtitleOutput::finish() { impl_->finish(); }
void SubtitleOutput::reset(const std::filesystem::path& destination) {
    impl_->finish();
    impl_ = std::make_unique<Impl>(destination, impl_->format, impl_->header);
}
}
