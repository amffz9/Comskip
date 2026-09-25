#pragma once
#include "exit_requested.h"
#include "media/video_packet_outcome.h"

namespace comskip {
// Maps decoder outcomes that end decoding onto the application's exit status.
inline void apply_decode_exit_policy(media::VideoPacketOutcome outcome)
{
    if (outcome == media::VideoPacketOutcome::selftest_complete) request_exit(1);
    if (outcome == media::VideoPacketOutcome::positioning_failure) request_exit(-1);
}
}
