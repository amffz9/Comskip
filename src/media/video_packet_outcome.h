#pragma once
namespace comskip::media {
enum class VideoPacketOutcome { no_frame, frame_decoded, analysis_complete, selftest_complete, positioning_failure };
enum class FrameSubmission { continue_decoding, analysis_complete, selftest_complete };
constexpr bool ends_decoding(VideoPacketOutcome outcome) noexcept {
    return outcome == VideoPacketOutcome::selftest_complete || outcome == VideoPacketOutcome::positioning_failure;
}
constexpr VideoPacketOutcome video_packet_outcome(bool frame_decoded, bool analysis_complete,
    bool selftest_complete, bool positioning_failure) noexcept {
    if (positioning_failure) return VideoPacketOutcome::positioning_failure;
    if (selftest_complete) return VideoPacketOutcome::selftest_complete;
    if (analysis_complete) return VideoPacketOutcome::analysis_complete;
    return frame_decoded ? VideoPacketOutcome::frame_decoded : VideoPacketOutcome::no_frame;
}
}
