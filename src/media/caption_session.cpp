#include "media/caption_session.h"
#include "media/a53_caption_bridge.h"
#include <stdexcept>
#include <utility>
#include <format>

namespace comskip::media {
CaptionSession::CaptionSession(CaptionOutputOptions options)
    : options_(std::move(options)), decoder_(options_.field) { open_outputs(); }
CaptionSession::~CaptionSession() {
    try { finish(last_time_.value_or(CaptionTimestamp{})); } catch (...) {}
}
void CaptionSession::open_outputs() {
    if (options_.srt) {
        auto filename = options_.basename; filename += ".srt";
        outputs_.emplace_back(filename, SubtitleFormat::srt, decoder_.ass_header());
    }
    if (options_.sami) {
        auto filename = options_.basename; filename += ".smi";
        outputs_.emplace_back(filename, SubtitleFormat::sami, decoder_.ass_header());
    }
}
void CaptionSession::write(std::span<const CaptionCue> cues) {
    for (const auto& cue : cues) for (auto& output : outputs_) output.write(cue);
}
void CaptionSession::consume(std::span<const std::uint8_t> a53, CaptionTimestamp timestamp) {
    if (finished_) throw std::logic_error("Caption session must be reset after EOF");
    if (last_time_ && timestamp < *last_time_)
        throw std::invalid_argument(std::format("Caption consume time {} precedes {}", timestamp.count(), last_time_->count()));
    const auto cues = decoder_.decode(a53, timestamp); write(cues); last_time_ = timestamp;
}
void CaptionSession::consume_stored_packet(std::span<const std::uint8_t> packet, CaptionTimestamp timestamp) {
    const auto a53 = extract_a53_captions(packet);
    if (!a53.empty()) consume(a53, timestamp);
}
void CaptionSession::finish(CaptionTimestamp end) {
    if (finished_) return;
    if (last_time_ && end < *last_time_)
        throw std::invalid_argument(std::format("Caption EOF time {} precedes {}", end.count(), last_time_->count()));
    const auto cues = decoder_.drain(end); write(cues);
    for (auto& output : outputs_) output.finish();
    last_time_ = end; finished_ = true;
}
void CaptionSession::reset() {
    // Replaced files are completed/closed before rebuilding fresh decoder state.
    outputs_.clear(); decoder_.reset(); last_time_.reset(); finished_ = false;
    open_outputs();
}
}
