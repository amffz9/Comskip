#pragma once
#include "audio_samples.h"
#include "ffmpeg_resources.h"
#include <optional>
#include <string>
struct VideoState {
    comskip::media::NetworkSession network;
    comskip::media::InputPtr pFormatCtx;
    comskip::media::CodecPtr dec_ctx, audio_ctx, subtitle_ctx;
    // MPEG2 uses field-time ticks; MPEG1 overrides this to one on opening.
    int ticks_per_frame{2};
    std::optional<int> videoStream;
    std::optional<int> audioStream;
    std::optional<int> subtitleStream;
    int seek_req{};
    int seek_by_bytes{};
    int seek_no_flush{};
    double seek_pts{};
    int seek_flags{};
    int64_t seek_pos{};
    double audio_clock{};
    AVStream* audio_st{};
    AVStream* subtitle_st{};
    double video_clock{}; ///<pts of last decoded frame / predicted pts of next decoded frame
    double video_clock_submitted{};
    AVStream* video_st{};
    comskip::media::FramePtr pFrame;
    std::string filename;
    int quit{};
    // True while live mode reads a local recording that is still growing.
    // End of input then means the writer stopped, not a place to reopen.
    bool follows_growing_input{};
    comskip::media::FramePtr frame;
    double duration{};
    double fps{};
    comskip::media::ScalerPtr img_convert_ctx;
    comskip::media::AudioNormalizer audio_normalizer;
};
