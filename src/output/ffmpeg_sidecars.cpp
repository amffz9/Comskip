#include "output/ffmpeg_sidecars.h"
#include "diagnostic.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
}
#include <cmath>
#include <format>
#include <memory>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace comskip::output {
namespace {
void validate(SidecarSeconds start, SidecarSeconds end) {
    if (!std::isfinite(start.count()) || !std::isfinite(end.count()) ||
        start.count() < 0 || end < start)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_ffmpeg_sidecar_interval);
}
std::int64_t centiseconds(SidecarSeconds time) {
    const double value = time.count() * 100;
    if (!std::isfinite(value) || value >= std::ldexp(1.0, 63))
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::ffmetadata_timestamp_exceeds_signed_range);
    return static_cast<std::int64_t>(value);
}
struct FormatCloser {
    void operator()(AVFormatContext* format) const noexcept {
        if (format->pb) {
            unsigned char* buffer = nullptr;
            avio_close_dyn_buf(format->pb, &buffer);
            av_free(buffer);
        }
        avformat_free_context(format);
    }
};
void checked(int status, comskip::diagnostics::Code operation) {
    if (status < 0) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(operation);
}
}
void write_ffmetadata(std::ostream& output, std::span<const SidecarChapter> chapters) {
    if (chapters.size() > std::numeric_limits<unsigned>::max())
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::too_many_ffmetadata_chapters);
    for (const auto& chapter : chapters) {
        validate(chapter.start, chapter.end);
        centiseconds(chapter.start); centiseconds(chapter.end);
        if (chapter.kind != SidecarSegmentKind::show && chapter.kind != SidecarSegmentKind::commercial)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_ffmetadata_segment_kind);
    }
    AVFormatContext* raw = nullptr;
    checked(avformat_alloc_output_context2(&raw, nullptr, "ffmetadata", nullptr),
        comskip::diagnostics::Code::cannot_create_ffmetadata_muxer);
    std::unique_ptr<AVFormatContext, FormatCloser> format(raw);
    // Suppress FFmpeg's version-dependent encoder tag; legacy sidecars contain
    // only the header and explicitly supplied chapter metadata.
    format->flags |= AVFMT_FLAG_BITEXACT;
    checked(avio_open_dyn_buf(&format->pb), comskip::diagnostics::Code::cannot_allocate_ffmetadata_buffer);
    if (!chapters.empty()) {
        format->chapters = static_cast<AVChapter**>(av_calloc(chapters.size(), sizeof(AVChapter*)));
        if (!format->chapters) throw std::bad_alloc{};
    }
    for (const auto& chapter : chapters) {
        auto* entry = static_cast<AVChapter*>(av_mallocz(sizeof(AVChapter)));
        if (!entry) throw std::bad_alloc{};
        format->chapters[format->nb_chapters++] = entry;
        entry->id = format->nb_chapters - 1;
        entry->time_base = {1, 100};
        entry->start = centiseconds(chapter.start);
        entry->end = centiseconds(chapter.end);
        checked(av_dict_set(&entry->metadata, "title",
            chapter.kind == SidecarSegmentKind::show ? "Show Segment" : "Commercial Segment", 0),
            comskip::diagnostics::Code::cannot_allocate_ffmetadata_chapter_title);
    }
    checked(avformat_write_header(format.get(), nullptr), comskip::diagnostics::Code::cannot_write_ffmetadata_header);
    checked(av_write_trailer(format.get()), comskip::diagnostics::Code::cannot_write_ffmetadata_chapters);
    unsigned char* raw_bytes = nullptr;
    const int size = avio_close_dyn_buf(format->pb, &raw_bytes);
    format->pb = nullptr;
    std::unique_ptr<unsigned char, decltype(&av_free)> bytes(raw_bytes, av_free);
    checked(size, comskip::diagnostics::Code::cannot_complete_ffmetadata_buffer);
    output.write(reinterpret_cast<const char*>(bytes.get()), size);
    if (!output) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_write_ffmetadata_output);
}
void write_ffsplit(std::ostream& output, std::span<const SidecarShowSegment> segments) {
    for (const auto& segment : segments) {
        validate(segment.start, segment.end);
        if (segment.number < 0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_ffsplit_segment_number);
    }
    for (const auto& segment : segments)
        output << std::format("-c copy -ss {:.3f} -t {:.3f} segment{:03}.ts \n",
            segment.start.count(), (segment.end - segment.start).count(), segment.number);
    if (!output) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_write_ffsplit_output);
}
}
