#include "media/subtitle_output.h"
#include "media/ffmpeg_resources.h"
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <bit>
#include <chrono>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>

namespace {
using namespace comskip::media;
using namespace std::chrono_literals;
class SubtitleOutputTest : public ::testing::Test {
protected:
    std::filesystem::path directory;
    CaptionDecoder decoder;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-subtitles-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
    }
    void TearDown() override { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    CaptionCue cue(std::string text, CaptionTimestamp start = 1s, CaptionTimestamp end = 3s) {
        return {start, end, {{"", "0,0,Default,,0,0,0,," + text}}};
    }
    std::string read(const std::filesystem::path& filename) {
        std::ifstream file(filename, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), {}};
    }
};
TEST_F(SubtitleOutputTest, SrtMuxesExactTimesAndPreservesUnicodeAndStyles) {
    const auto filename = directory / std::filesystem::path(u8"é 字幕.srt");
    SubtitleOutput output(filename, SubtitleFormat::srt, decoder.ass_header());
    output.write(cue("{\\i1}Á & text{\\i0}\\Nsecond line", 1250ms, 3500ms));
    output.finish(); output.finish();
    const auto text = read(filename);
    EXPECT_TRUE(text.starts_with("1\n00:00:01,250 --> 00:00:03,500\n"));
    EXPECT_TRUE(text.contains("<i>Á & text</i>"));
    EXPECT_TRUE(text.contains("\nsecond line"));
    EXPECT_THROW(output.write(cue("after EOF", 4s, 5s)), std::logic_error);
}
TEST_F(SubtitleOutputTest, SamiUsesEscapedUnicodeMarkupAndExplicitClearTime) {
    const auto filename = directory / "captions.smi";
    SubtitleOutput output(filename, SubtitleFormat::sami, decoder.ass_header());
    output.write(cue("{\\i1}Á & <literal>{\\i0}\\Nnext", 1250ms, 3500ms)); output.finish();
    pugi::xml_document document;
    ASSERT_TRUE(document.load_file(filename.c_str()));
    auto body = document.child("SAMI").child("BODY");
    EXPECT_EQ(body.child("SYNC").attribute("Start").as_int(), 1250);
    auto paragraph = body.child("SYNC").child("P");
    EXPECT_STREQ(document.select_node("/SAMI/BODY/SYNC[1]/P//i").node().text().get(), "Á & <literal>");
    EXPECT_TRUE(document.select_node("/SAMI/BODY/SYNC[1]/P//BR"));
    EXPECT_EQ(body.last_child().attribute("Start").as_int(), 3500);
    EXPECT_STREQ(body.last_child().child("P").text().get(), "\xc2\xa0");
    EXPECT_FALSE(read(filename).contains("<literal>"));
    EXPECT_FALSE(read(filename).contains("{\\an"));
}
TEST_F(SubtitleOutputTest, InterleavedDestinationsAndResetOwnIndependentNumbering) {
    SubtitleOutput a(directory / "a.srt", SubtitleFormat::srt, decoder.ass_header());
    SubtitleOutput b(directory / "b.srt", SubtitleFormat::srt, decoder.ass_header());
    a.write(cue("alpha")); b.write(cue("beta"));
    a.write(cue("alpha two", 4s, 5s)); b.finish();
    a.reset(directory / "c.srt"); a.write(cue("reset", 1s, 2s)); a.finish();
    EXPECT_TRUE(read(directory / "a.srt").contains("2\n00:00:04,000"));
    EXPECT_TRUE(read(directory / "b.srt").contains("beta"));
    EXPECT_FALSE(read(directory / "b.srt").contains("alpha"));
    EXPECT_TRUE(read(directory / "c.srt").starts_with("1\n"));
    EXPECT_FALSE(read(directory / "c.srt").contains("alpha"));
    // All library I/O handles are closed by finish/reset, while writers live.
    EXPECT_TRUE(std::filesystem::remove(directory / "a.srt"));
    EXPECT_TRUE(std::filesystem::remove(directory / "b.srt"));
    EXPECT_TRUE(std::filesystem::remove(directory / "c.srt"));
}
TEST_F(SubtitleOutputTest, FailedDestinationAndInvalidCuesDoNotPoisonOtherWriter) {
    EXPECT_THROW(SubtitleOutput(directory / "missing" / "bad.srt", SubtitleFormat::srt, decoder.ass_header()), std::runtime_error);
    EXPECT_THROW(SubtitleOutput(directory / "missing" / "bad.smi", SubtitleFormat::sami, decoder.ass_header()), std::ios_base::failure);
    SubtitleOutput output(directory / "good.srt", SubtitleFormat::srt, decoder.ass_header());
    EXPECT_THROW(output.write(cue("negative", -1s, 2s)), std::invalid_argument);
    EXPECT_THROW(output.write(cue("reverse", 3s, 2s)), std::invalid_argument);
    EXPECT_THROW(output.write({1s, 2s, {{"no ASS", ""}}}), std::invalid_argument);
    output.write(cue("good"));
    EXPECT_THROW(output.write(cue("overlap", 2s, 4s)), std::invalid_argument);
    output.write(cue("next", 4s, 5s)); output.finish();
    EXPECT_TRUE(read(directory / "good.srt").contains("2\n00:00:04,000"));
}
TEST_F(SubtitleOutputTest, DecoderEofCueAndDestructorCompleteSamiFile) {
    const auto parity = [](std::uint8_t value) {
        return static_cast<std::uint8_t>(value | ((std::popcount(static_cast<unsigned>(value)) % 2 == 0) ? 0x80 : 0));
    };
    const std::vector<std::uint8_t> loading{0xfc, parity(0x14), parity(0x20), 0xfc, parity('H'), parity('i')};
    const std::vector<std::uint8_t> show{0xfc, parity(0x14), parity(0x2f)};
    decoder.decode(loading, 1s); decoder.decode(show, 2s);
    const auto cues = decoder.drain(6s); ASSERT_EQ(cues.size(), 1u);
    {
        SubtitleOutput output(directory / "eof.smi", SubtitleFormat::sami, decoder.ass_header());
        output.write(cues[0]);
    }
    pugi::xml_document document; ASSERT_TRUE(document.load_file((directory / "eof.smi").c_str()));
    EXPECT_EQ(document.child("SAMI").child("BODY").last_child().attribute("Start").as_int(), 6000);
    EXPECT_TRUE(read(directory / "eof.smi").contains("Hi"));
}
TEST_F(SubtitleOutputTest, LongSubtitleRetriesEncoderBufferWithoutTruncation) {
    SubtitleOutput output(directory / "long.srt", SubtitleFormat::srt, decoder.ass_header());
    const std::string text(20000, 'x'); output.write(cue(text)); output.finish();
    EXPECT_TRUE(read(directory / "long.srt").contains(text));
}
TEST_F(SubtitleOutputTest, GeneratedSamiIsReadableByFfmpegWithExactCueDuration) {
    const auto filename = directory / "readable.smi";
    SubtitleOutput output(filename, SubtitleFormat::sami, decoder.ass_header());
    output.write(cue("{\\i1}readable{\\i0}", 1250ms, 3500ms)); output.finish();
    AVFormatContext* raw = nullptr;
    const auto name = filename.u8string();
    const std::string bytes(reinterpret_cast<const char*>(name.data()), name.size());
    ASSERT_GE(avformat_open_input(&raw, bytes.c_str(), nullptr, nullptr), 0);
    InputPtr input(raw);
    auto packet = make_packet();
    ASSERT_GE(av_read_frame(input.get(), packet.get()), 0);
    const auto timebase = input->streams[packet->stream_index]->time_base;
    EXPECT_EQ(av_rescale_q(packet->pts, timebase, AVRational{1, 1000}), 1250);
    EXPECT_EQ(av_rescale_q(packet->duration, timebase, AVRational{1, 1000}), 2250);
    EXPECT_TRUE(std::string(reinterpret_cast<const char*>(packet->data), packet->size).contains("readable"));
}
}
