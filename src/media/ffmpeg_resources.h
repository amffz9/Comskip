#pragma once
#include <memory>
#include <new>
#include <stdexcept>
#include <array>
#include "diagnostic.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace comskip::media {
class NetworkSession {
public:
    NetworkSession() {
        const int result = avformat_network_init();
        if (result < 0) {
            std::array<char, AV_ERROR_MAX_STRING_SIZE> detail{};
            av_strerror(result, detail.data(), detail.size());
            throw diagnostics::DiagnosticError<std::runtime_error>(
                diagnostics::Code::cannot_initialize_ffmpeg_networking_detail, {detail.data()});
        }
    }
    ~NetworkSession() { avformat_network_deinit(); }
    NetworkSession(const NetworkSession&) = delete;
    NetworkSession& operator=(const NetworkSession&) = delete;
};
struct FrameDeleter { void operator()(AVFrame* value) const noexcept { av_frame_free(&value); } };
struct PacketDeleter { void operator()(AVPacket* value) const noexcept { av_packet_free(&value); } };
struct CodecDeleter { void operator()(AVCodecContext* value) const noexcept { avcodec_free_context(&value); } };
struct CodecParametersDeleter { void operator()(AVCodecParameters* value) const noexcept { avcodec_parameters_free(&value); } };
struct InputDeleter { void operator()(AVFormatContext* value) const noexcept { avformat_close_input(&value); } };
struct OutputFormatDeleter {
    void operator()(AVFormatContext* value) const noexcept {
        if (!value) return;
        if (value->pb) avio_closep(&value->pb);
        avformat_free_context(value);
    }
};
struct DynamicOutputFormatDeleter {
    void operator()(AVFormatContext* value) const noexcept {
        if (!value) return;
        if (value->pb) {
            unsigned char* buffer = nullptr;
            avio_close_dyn_buf(value->pb, &buffer);
            av_free(buffer);
        }
        avformat_free_context(value);
    }
};
struct DictionaryDeleter { void operator()(AVDictionary* value) const noexcept { av_dict_free(&value); } };
struct BufferDeleter { void operator()(unsigned char* value) const noexcept { av_free(value); } };
struct ScalerDeleter { void operator()(SwsContext* value) const noexcept { sws_freeContext(value); } };
struct ResamplerDeleter { void operator()(SwrContext* value) const noexcept { swr_free(&value); } };
struct SubtitleOwner {
    AVSubtitle value{};
    SubtitleOwner() = default;
    SubtitleOwner(const SubtitleOwner&) = delete;
    SubtitleOwner& operator=(const SubtitleOwner&) = delete;
    ~SubtitleOwner() noexcept { avsubtitle_free(&value); }
};
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
using CodecParametersPtr = std::unique_ptr<AVCodecParameters, CodecParametersDeleter>;
using InputPtr = std::unique_ptr<AVFormatContext, InputDeleter>;
using OutputFormatPtr = std::unique_ptr<AVFormatContext, OutputFormatDeleter>;
using DynamicOutputFormatPtr = std::unique_ptr<AVFormatContext, DynamicOutputFormatDeleter>;
using DictionaryPtr = std::unique_ptr<AVDictionary, DictionaryDeleter>;

// std::inout_ptr is C++23, but libstdc++ did not provide it until GCC 14.
// Keep ownership around FFmpeg's pointer-to-pointer APIs on older toolchains.
template<class SmartPointer>
class InOutPtr {
public:
    using pointer = typename SmartPointer::pointer;

    explicit InOutPtr(SmartPointer& owner) noexcept
        : owner_(owner), pointer_(owner.release()) {}
    InOutPtr(const InOutPtr&) = delete;
    InOutPtr& operator=(const InOutPtr&) = delete;
    ~InOutPtr() { owner_.reset(pointer_); }

    operator pointer*() noexcept { return &pointer_; }

private:
    SmartPointer& owner_;
    pointer pointer_;
};

template<class SmartPointer>
InOutPtr<SmartPointer> inout_ptr(SmartPointer& owner) noexcept {
    return InOutPtr<SmartPointer>(owner);
}
using BufferPtr = std::unique_ptr<unsigned char, BufferDeleter>;
using ScalerPtr = std::unique_ptr<SwsContext, ScalerDeleter>;
using ResamplerPtr = std::unique_ptr<SwrContext, ResamplerDeleter>;
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
