#pragma once
#include "localization/diagnostic.h"
#include <array>
extern "C" {
#include <libavutil/error.h>
}

namespace comskip::media {
enum class VideoReceiveStatus { frame, try_again, end_of_stream };

inline std::string video_decode_error_detail(int status) {
    std::array<char,AV_ERROR_MAX_STRING_SIZE> detail{};
    av_strerror(status,detail.data(),detail.size());
    return detail.data();
}
inline void require_video_packet_sent(int status) {
    if (status < 0)
        throw diagnostics::DiagnosticError<std::runtime_error>(
            diagnostics::Code::send_video_packet_detail,{video_decode_error_detail(status)});
}
inline VideoReceiveStatus classify_video_receive_status(int status) {
    if (status >= 0) return VideoReceiveStatus::frame;
    if (status == AVERROR(EAGAIN)) return VideoReceiveStatus::try_again;
    if (status == AVERROR_EOF) return VideoReceiveStatus::end_of_stream;
    throw diagnostics::DiagnosticError<std::runtime_error>(
        diagnostics::Code::receive_video_frame_detail,{video_decode_error_detail(status)});
}
}
