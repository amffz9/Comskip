#pragma once
#include <memory>
#include <new>
#include <stdexcept>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace comskip::media {
class NetworkSession {
public:
    NetworkSession() {
        if (avformat_network_init() < 0)
            throw std::runtime_error("Cannot initialize FFmpeg networking");
    }
    ~NetworkSession() { avformat_network_deinit(); }
    NetworkSession(const NetworkSession&) = delete;
    NetworkSession& operator=(const NetworkSession&) = delete;
};
struct FrameDeleter { void operator()(AVFrame* value) const noexcept { av_frame_free(&value); } };
struct PacketDeleter { void operator()(AVPacket* value) const noexcept { av_packet_free(&value); } };
struct CodecDeleter { void operator()(AVCodecContext* value) const noexcept { avcodec_free_context(&value); } };
struct InputDeleter { void operator()(AVFormatContext* value) const noexcept { avformat_close_input(&value); } };
struct DictionaryDeleter { void operator()(AVDictionary* value) const noexcept { av_dict_free(&value); } };
struct ScalerDeleter { void operator()(SwsContext* value) const noexcept { sws_freeContext(value); } };
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
using InputPtr = std::unique_ptr<AVFormatContext, InputDeleter>;
using DictionaryPtr = std::unique_ptr<AVDictionary, DictionaryDeleter>;
using ScalerPtr = std::unique_ptr<SwsContext, ScalerDeleter>;
inline FramePtr make_frame() {
    FramePtr value(av_frame_alloc());
    if (!value) throw std::bad_alloc();
    return value;
}
inline PacketPtr make_packet() {
    PacketPtr value(av_packet_alloc());
    if (!value) throw std::bad_alloc();
    return value;
}
}
