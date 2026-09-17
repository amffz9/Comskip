#include "../localization/diagnostic.h"
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
using OutputPtr = OutputFormatPtr;
std::string utf8(const std::filesystem::path& path) {
    auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
void check(int status, comskip::diagnostics::Code operation, std::vector<std::string> arguments = {}) {
    if (status < 0) {
        char detail[AV_ERROR_MAX_STRING_SIZE]; av_strerror(status, detail, sizeof(detail));
        arguments.emplace_back(detail);
        throw comskip::diagnostics::DiagnosticError<std::runtime_error>(operation, std::move(arguments));
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
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::subtitle_output_requires_an_ass_header);
        const auto* codec = avcodec_find_encoder(AV_CODEC_ID_SUBRIP);
        if (!codec) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::ffmpeg_subrip_encoder_is_unavailable);
        encoder.reset(avcodec_alloc_context3(codec)); if (!encoder) throw std::bad_alloc{};
        encoder->time_base = {1, 1000000};
        encoder->subtitle_header = static_cast<std::uint8_t*>(av_mallocz(header.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!encoder->subtitle_header) throw std::bad_alloc{};
        std::copy(header.begin(), header.end(), encoder->subtitle_header);
        encoder->subtitle_header_size = static_cast<int>(header.size());
        check(avcodec_open2(encoder.get(), codec, nullptr), comskip::diagnostics::Code::opening_subtitle_encoder_detail);
        if (format == SubtitleFormat::srt) {
            AVFormatContext* output = nullptr;
            const auto filename = utf8(destination);
            const int allocated = avformat_alloc_output_context2(&output, nullptr, "srt", filename.c_str());
            muxer.reset(output);
            check(allocated, comskip::diagnostics::Code::creating_subtitle_muxer_detail); if (!muxer) throw std::bad_alloc{};
            stream = avformat_new_stream(muxer.get(), nullptr); if (!stream) throw std::bad_alloc{};
            stream->time_base = {1, 1000000};
            check(avcodec_parameters_from_context(stream->codecpar, encoder.get()), comskip::diagnostics::Code::configuring_subtitle_stream_detail);
            check(avio_open(&muxer->pb, filename.c_str(), AVIO_FLAG_WRITE), comskip::diagnostics::Code::opening_subtitle_destination_detail, {filename});
            check(avformat_write_header(muxer.get(), nullptr), comskip::diagnostics::Code::writing_subtitle_header_detail);
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
            if (cue.regions[i].ass.empty()) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::subtitle_region_requires_ass_data);
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
                    throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::subtitle_event_is_too_large);
                buffer.resize(buffer.size() * 2); continue;
            }
            check(size, comskip::diagnostics::Code::encoding_subtitle_event_detail);
            return {reinterpret_cast<const char*>(buffer.data()), static_cast<std::size_t>(size)};
        }
    }
    void write(const CaptionCue& cue) {
        if (finished) throw comskip::diagnostics::DiagnosticError<std::logic_error>(comskip::diagnostics::Code::subtitle_output_must_be_reset_after_completion);
        if (cue.start.count() < 0 || cue.end <= cue.start || (last_end && cue.start < *last_end))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::subtitle_cues_must_be_nonnegative_ordered_and_nonoverlapping);
        const auto text = encode(cue, format == SubtitleFormat::sami);
        if (!text.empty()) {
            if (format == SubtitleFormat::srt) {
                auto packet = make_packet();
                check(av_new_packet(packet.get(), static_cast<int>(text.size())), comskip::diagnostics::Code::allocating_subtitle_packet_detail);
                std::copy(text.begin(), text.end(), packet->data);
                packet->stream_index = stream->index;
                packet->pts = packet->dts = av_rescale_q(cue.start.count(), AVRational{1, 1000000}, stream->time_base);
                packet->duration = av_rescale_q(cue.end.count(), AVRational{1, 1000000}, stream->time_base) - packet->pts;
                check(av_interleaved_write_frame(muxer.get(), packet.get()), comskip::diagnostics::Code::writing_subtitle_packet_detail);
                avio_flush(muxer->pb); check(muxer->pb->error, comskip::diagnostics::Code::flushing_subtitle_packet_detail);
            } else {
                pugi::xml_document fragment;
                const auto source = "<P>" + text + "</P>";
                if (!fragment.load_string(source.c_str(), pugi::parse_default | pugi::parse_ws_pcdata))
                    throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::ffmpeg_subtitle_markup_cannot_be_represented_as_sami);
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
            check(av_write_trailer(muxer.get()), comskip::diagnostics::Code::completing_subtitle_file_detail);
            avio_flush(muxer->pb); check(muxer->pb->error, comskip::diagnostics::Code::flushing_subtitle_file_detail);
            const int closed = avio_closep(&muxer->pb);
            finished = true;
            check(closed, comskip::diagnostics::Code::closing_subtitle_file_detail);
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
