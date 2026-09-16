#include "recording_context.h"
#include "media/ffmpeg_resources.h"
#include "exit_requested.h"

#include <gtest/gtest.h>
#include <bit>
#include <chrono>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>

int comskip_main(RecordingContext&, int, char**);
namespace {
using namespace comskip::media;
std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string(); return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Missing caption analysis output");
    return {std::istreambuf_iterator<char>(file), {}};
}
std::uint8_t parity(std::uint8_t value) {
    return static_cast<std::uint8_t>(value | (std::popcount(static_cast<unsigned>(value)) % 2 == 0 ? 0x80 : 0));
}
void fixture(const std::filesystem::path& path, std::string_view text, bool early_display = false) {
    const auto* encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    if (!encoder) throw std::runtime_error("MPEG2 fixture encoder unavailable");
    CodecPtr codec(avcodec_alloc_context3(encoder)); if (!codec) throw std::bad_alloc{};
    codec->width = 160; codec->height = 120; codec->pix_fmt = AV_PIX_FMT_YUV420P;
    codec->time_base = {1, 25}; codec->framerate = {25, 1};
    codec->gop_size = 12; codec->max_b_frames = 0; codec->bit_rate = 500000;
    if (avcodec_open2(codec.get(), encoder, nullptr) < 0) throw std::runtime_error("Cannot encode fixture");
    auto frame = make_frame(); frame->width = codec->width; frame->height = codec->height; frame->format = codec->pix_fmt;
    if (av_frame_get_buffer(frame.get(), 0) < 0) throw std::bad_alloc{};
    auto packet = make_packet();
    std::ofstream file(path, std::ios::binary); file.exceptions(std::ios::failbit | std::ios::badbit);
    const auto receive = [&] {
        while (true) {
            const int status = avcodec_receive_packet(codec.get(), packet.get());
            if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) break;
            if (status < 0) throw std::runtime_error("Fixture packet encoding failed");
            file.write(reinterpret_cast<const char*>(packet->data), packet->size); av_packet_unref(packet.get());
        }
    };
    for (int index = 0; index < 150; ++index) {
        if (av_frame_make_writable(frame.get()) < 0) throw std::bad_alloc{};
        av_frame_remove_side_data(frame.get(), AV_FRAME_DATA_A53_CC);
        for (int row = 0; row < frame->height; ++row)
            for (int column = 0; column < frame->width; ++column)
                frame->data[0][row * frame->linesize[0] + column] = index % 100 < 4 ? 16 : 40 + (column + index) % 160;
        for (int plane = 1; plane < 3; ++plane)
            for (int row = 0; row < frame->height / 2; ++row)
                std::fill_n(frame->data[plane] + row * frame->linesize[plane], frame->width / 2, 128);
        std::vector<std::uint8_t> captions;
        const auto pair = [&](std::uint8_t hi, std::uint8_t lo) {
            captions.insert(captions.end(), {0xfc, parity(hi), parity(lo)});
        };
        if (index == 20 || index == 120 || (early_display && index == 0)) {
            pair(0x14, 0x20); pair(0x14, 0x2e); pair(0x14, 0x60);
            for (std::size_t position = 0; position < text.size(); position += 2)
                pair(text[position], position + 1 < text.size() ? text[position + 1] : 0);
            if (index == 120 || (early_display && index == 0)) pair(0x14, 0x2f); // Outstanding display reaches EOF/reset.
        } else if (index == 25) pair(0x14, 0x2f);
        else if (index == 100) pair(0x14, 0x2c);
        if (!captions.empty()) {
            auto* side = av_frame_new_side_data(frame.get(), AV_FRAME_DATA_A53_CC, captions.size());
            if (!side) throw std::bad_alloc{};
            std::copy(captions.begin(), captions.end(), side->data);
        }
        frame->pts = index;
        if (avcodec_send_frame(codec.get(), frame.get()) < 0) throw std::runtime_error("Cannot send fixture frame");
        receive();
    }
    if (avcodec_send_frame(codec.get(), nullptr) < 0) throw std::runtime_error("Cannot drain fixture encoder");
    receive();
}
class CaptionAnalysis : public ::testing::Test {
protected:
    std::filesystem::path directory, ini;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-caption-analysis-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        ini = directory / "settings.ini";
        std::ofstream settings(ini);
        settings << "detect_method=1\nnum_logo_buffers=2\noutput_srt=1\noutput_smi=1\noutput_framearray=1\noutput_data=1\n"
                    "output_edl=1\nlive_tv_retries=0\nadded_recording=0\nverbose=0\n";
    }
    void TearDown() override { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    int analyze(RecordingContext& context, const std::filesystem::path& input, std::string_view label, int threads = 1, int selftest = 0) {
        auto destination = directory / label; std::filesystem::create_directory(destination);
        std::vector<std::string> arguments{"caption-analysis", "--ini=" + utf8(ini), "--output=" + utf8(destination),
            "--threads=" + std::to_string(threads), utf8(input)};
        if (selftest) arguments.insert(arguments.end() - 1, "--selftest=" + std::to_string(selftest));
        std::vector<char*> argv; for (auto& value : arguments) argv.push_back(value.data()); argv.push_back(nullptr);
        try { return comskip_main(context, static_cast<int>(arguments.size()), argv.data()); }
        catch (const comskip::ExitRequested& requested) { return requested.status(); }
    }
};
TEST_F(CaptionAnalysis, ActualA53RecordingsProduceIndependentSubtitlesAndDrainEof) {
    fixture(directory / "alpha.m2v", "ALPHA"); fixture(directory / "beta.m2v", "BETA");
    auto first = std::make_unique<RecordingContext>(), second = std::make_unique<RecordingContext>();
    EXPECT_LE(analyze(*first, directory / "alpha.m2v", "alpha1"), 1);
    const auto alpha = read(directory / "alpha1/alpha.srt");
    EXPECT_TRUE(alpha.contains("ALPHA")); EXPECT_FALSE(alpha.contains("BETA"));
    EXPECT_TRUE(alpha.contains("2\n")); // Outstanding second display reaches EOF.
    EXPECT_FALSE(first->captions);
    EXPECT_LE(analyze(*second, directory / "beta.m2v", "beta1", 4), 1);
    const auto beta = read(directory / "beta1/beta.srt");
    EXPECT_TRUE(beta.contains("BETA")); EXPECT_FALSE(beta.contains("ALPHA")); EXPECT_FALSE(second->captions);
    auto repeated = std::make_unique<RecordingContext>();
    EXPECT_LE(analyze(*repeated, directory / "alpha.m2v", "alpha2", 4), 1);
    EXPECT_EQ(read(directory / "alpha2/alpha.srt"), alpha);
    EXPECT_EQ(read(directory / "alpha2/alpha.smi"), read(directory / "alpha1/alpha.smi"));
    // Close subtitle resources before returning, even while recording contexts live.
    EXPECT_TRUE(std::filesystem::remove(directory / "alpha1/alpha.srt"));
    EXPECT_TRUE(std::filesystem::remove(directory / "beta1/beta.smi"));
}
TEST_F(CaptionAnalysis, FailedAnalysisUnwindsOwnedOutputsThenNextRecordingWorks) {
    auto failed = std::make_unique<RecordingContext>();
    EXPECT_EQ(analyze(*failed, directory / "missing.m2v", "missing"), -1);
    EXPECT_FALSE(failed->captions);
    fixture(directory / "success.m2v", "GOOD");
    auto success = std::make_unique<RecordingContext>();
    EXPECT_LE(analyze(*success, directory / "success.m2v", "success"), 1);
    EXPECT_TRUE(read(directory / "success/success.srt").contains("GOOD"));
}
TEST_F(CaptionAnalysis, ActualInputReopenClearsPendingSubtitleDisplayAndClosesOutputs) {
    fixture(directory / "reset.m2v", "RESET", true);
    auto reopened = std::make_unique<RecordingContext>();
    EXPECT_EQ(analyze(*reopened, directory / "reset.m2v", "reopened", 1, 2), 1);
    EXPECT_GT(reopened->state.pass, 0);
    EXPECT_FALSE(reopened->captions);
    EXPECT_TRUE(read(directory / "reopened/reset.srt").empty());
    EXPECT_TRUE(std::filesystem::remove(directory / "reopened/reset.srt"));
    auto next = std::make_unique<RecordingContext>();
    EXPECT_LE(analyze(*next, directory / "reset.m2v", "next"), 1);
    EXPECT_TRUE(read(directory / "next/reset.srt").contains("RESET"));
}
TEST_F(CaptionAnalysis, CsvCompanionCaptionReplayMatchesActualDecoderOutput) {
    fixture(directory / "source.m2v", "REPLAY");
    auto original = std::make_unique<RecordingContext>();
    EXPECT_LE(analyze(*original, directory / "source.m2v", "original"), 1);
    const auto expected = read(directory / "original/source.srt");
    auto replay = std::make_unique<RecordingContext>();
    EXPECT_LE(analyze(*replay, directory / "original/source.csv", "replayed"), 1);
    EXPECT_FALSE(replay->captions);
    // Legacy CSV contains frames 1..count-1, so its represented duration ends
    // one frame before the decoded recording. All cue content and preceding
    // timestamps must still match exactly.
    auto expected_replay = expected;
    const auto eof = expected_replay.find("00:00:06,000");
    ASSERT_NE(eof, std::string::npos);
    expected_replay.replace(eof, 12, "00:00:05,960");
    EXPECT_EQ(read(directory / "replayed/source.srt"), expected_replay);
}
TEST_F(CaptionAnalysis, MalformedPersistedCaptionLengthsRejectBeforeObservations) {
    fixture(directory / "source.m2v", "SAFE");
    auto original = std::make_unique<RecordingContext>();
    EXPECT_LE(analyze(*original, directory / "source.m2v", "original"), 1);
    const auto companion = directory / "original/source.data";
    for (const auto& [label, payload] : std::vector<std::pair<std::string, std::string>>{
        {"oversized", "0000001:0501"}, {"negative", "0000001:-001"},
        {"shortlength", "0000001:01"}, {"shortpayload", "0000001:0011GA94"}}) {
        { std::ofstream file(companion, std::ios::binary | std::ios::trunc); file << payload; }
        auto context = std::make_unique<RecordingContext>();
        EXPECT_THROW(analyze(*context, directory / "original/source.csv", label), std::invalid_argument) << label;
        EXPECT_FALSE(context->captions) << label;
        EXPECT_TRUE(context->state.cc_text.empty() || context->state.cc_text[0].text_len == 0) << label;
    }
}
}
