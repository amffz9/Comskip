#include "../localization/diagnostic.h"
#include "media/caption_session.h"
#include "media/a53_caption_bridge.h"
#include <stdexcept>
#include <utility>
#include <format>
#include <algorithm>
#include <set>

namespace comskip::media {
CaptionSession::CaptionSession(CaptionOutputOptions options)
    : options_(std::move(options)), decoder_(options_.field) { open_outputs(); }
CaptionSession::~CaptionSession() {
    try { finish(last_time_.value_or(CaptionTimestamp{})); } catch (...) {}
}
void CaptionSession::open_outputs() {
    if (options_.srt) {
        auto filename = options_.basename; filename += ".srt";
        outputs_.emplace_back(filename, SubtitleFormat::srt, stream_decoder_ ? stream_decoder_->ass_header() : decoder_.ass_header());
    }
    if (options_.sami) {
        auto filename = options_.basename; filename += ".smi";
        outputs_.emplace_back(filename, SubtitleFormat::sami, stream_decoder_ ? stream_decoder_->ass_header() : decoder_.ass_header());
    }
}
void CaptionSession::write(std::span<const CaptionCue> cues) {
    for (const auto& cue : cues) for (auto& output : outputs_) output.write(cue);
}
void CaptionSession::consume(std::span<const std::uint8_t> a53, CaptionTimestamp timestamp) {
    if (stream_decoder_) return;
    if (finished_) throw comskip::diagnostics::DiagnosticError<std::logic_error>(comskip::diagnostics::Code::caption_session_must_be_reset_after_eof);
    if (last_time_ && timestamp < *last_time_)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::caption_consume_time_precedes_previous, {std::to_string(timestamp.count()), std::to_string(last_time_->count())});
    const auto cues = decoder_.decode(a53, timestamp); write(cues); last_time_ = timestamp;
}
void CaptionSession::select_stream(const AVCodecParameters& parameters, AVRational time_base) {
    auto decoder = std::make_unique<SubtitleStreamDecoder>(parameters, time_base);
    if (!stream_decoder_) {
        outputs_.clear();
        stream_decoder_ = std::move(decoder);
        open_outputs();
    } else {
        // Reopening the same recording preserves completed cues/destinations.
        if (decoder->ass_header() != stream_decoder_->ass_header())
            throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::standalone_subtitle_header_changed_while_reopening_the_recording);
        stream_decoder_ = std::move(decoder);
    }
}
void CaptionSession::consume_stream(std::span<const std::uint8_t> packet, std::int64_t pts,
                                    std::int64_t duration, CaptionTimestamp recording_origin) {
    if (finished_) throw comskip::diagnostics::DiagnosticError<std::logic_error>(comskip::diagnostics::Code::caption_session_must_be_reset_after_eof);
    if (!stream_decoder_) throw comskip::diagnostics::DiagnosticError<std::logic_error>(comskip::diagnostics::Code::standalone_subtitle_stream_has_not_been_selected);
    stream_origin_ = recording_origin;
    auto cues = stream_decoder_->decode(packet, pts, duration);
    for (auto& cue : cues) {
        cue.start -= recording_origin;
        cue.end -= recording_origin;
        if (cue.end <= CaptionTimestamp{}) continue;
        cue.start = std::max(cue.start, CaptionTimestamp{});
        stream_cues_.push_back(std::move(cue));
    }
}
void CaptionSession::finish_stream_cues() {
    // Text streams may overlap or arrive out of display order. Sweep their
    // start/end events into complete, ordered screens for both output formats.
    struct Event { CaptionTimestamp time; std::size_t cue; bool start; };
    std::vector<Event> events;
    if (stream_cues_.size() > events.max_size() / 2)
        throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::too_many_standalone_subtitle_cues);
    events.reserve(stream_cues_.size() * 2);
    for (std::size_t i = 0; i < stream_cues_.size(); ++i) {
        events.push_back({stream_cues_[i].start, i, true});
        events.push_back({stream_cues_[i].end, i, false});
    }
    std::ranges::sort(events, {}, &Event::time);
    std::set<std::size_t> active;
    CaptionTimestamp previous{};
    for (std::size_t i = 0; i < events.size();) {
        const auto time = events[i].time;
        if (time > previous && !active.empty()) {
            CaptionCue screen{previous, time, {}};
            for (const auto cue : active)
                screen.regions.insert(screen.regions.end(), stream_cues_[cue].regions.begin(), stream_cues_[cue].regions.end());
            write({&screen, 1});
        }
        while (i < events.size() && events[i].time == time) {
            if (events[i].start) active.insert(events[i].cue);
            else active.erase(events[i].cue);
            ++i;
        }
        previous = time;
    }
    stream_cues_.clear();
}
void CaptionSession::consume_stored_packet(std::span<const std::uint8_t> packet, CaptionTimestamp timestamp) {
    const auto a53 = extract_a53_captions(packet);
    if (!a53.empty()) consume(a53, timestamp);
}
void CaptionSession::finish(CaptionTimestamp end) {
    if (finished_) return;
    if (stream_decoder_) {
        auto cues = stream_decoder_->drain();
        for (auto& cue : cues) {
            cue.start -= stream_origin_; cue.end -= stream_origin_;
            if (cue.end <= CaptionTimestamp{}) continue;
            cue.start = std::max(cue.start, CaptionTimestamp{});
            stream_cues_.push_back(std::move(cue));
        }
        finish_stream_cues();
        for (auto& output : outputs_) output.finish();
        finished_ = true;
        return;
    }
    if (last_time_ && end < *last_time_)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::caption_eof_time_precedes_previous, {std::to_string(end.count()), std::to_string(last_time_->count())});
    const auto cues = decoder_.drain(end); write(cues);
    for (auto& output : outputs_) output.finish();
    last_time_ = end; finished_ = true;
}
void CaptionSession::reset() {
    // Replaced files are completed/closed before rebuilding fresh decoder state.
    outputs_.clear(); decoder_.reset(); last_time_.reset(); finished_ = false;
    if (stream_decoder_) stream_decoder_->reset();
    stream_cues_.clear();
    open_outputs();
}
}
