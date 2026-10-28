#include "fingerprint/audio_stream.h"
#include <gtest/gtest.h>
#include <bit>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace comskip::fingerprint;

struct Recorder final : AudioStreamEvents {
    std::string log;
    std::vector<std::int16_t> samples;
    void run_start(double start) override { log += "R" + std::to_string(start) + " "; }
    void audio(std::span<const std::int16_t> values) override {
        log += "A" + std::to_string(values.size()) + " ";
        samples.insert(samples.end(), values.begin(), values.end());
    }
    void end() override { log += "E"; }
};

void append_header(std::vector<std::byte>& stream, char type, std::uint32_t size) {
    stream.push_back(static_cast<std::byte>(type));
    for (int shift = 0; shift < 32; shift += 8) stream.push_back(static_cast<std::byte>(size >> shift));
}
void append_run(std::vector<std::byte>& stream, double start) {
    append_header(stream, 'R', 8);
    const auto bits = std::bit_cast<std::uint64_t>(start);
    for (int shift = 0; shift < 64; shift += 8) stream.push_back(static_cast<std::byte>(bits >> shift));
}
void append_audio(std::vector<std::byte>& stream, const std::vector<std::int16_t>& samples) {
    append_header(stream, 'A', static_cast<std::uint32_t>(2 * samples.size()));
    for (const auto sample : samples) {
        stream.push_back(static_cast<std::byte>(static_cast<std::uint16_t>(sample) & 0xff));
        stream.push_back(static_cast<std::byte>(static_cast<std::uint16_t>(sample) >> 8));
    }
}

void push_all(AudioStreamParser& parser, const std::vector<std::byte>& stream, std::size_t piece) {
    for (std::size_t offset = 0; offset < stream.size(); offset += piece)
        parser.push(std::span(stream).subspan(offset, std::min(piece, stream.size() - offset)));
}

TEST(AudioStream, ReadsRunsAudioAndEndInPiecesOfAnySize) {
    std::vector<std::byte> stream;
    append_run(stream, 1234.5);
    append_audio(stream, {1, -2, 32767, -32768});
    append_audio(stream, {});
    append_run(stream, 2000.25);
    append_audio(stream, {5});
    append_header(stream, 'E', 0);
    for (const std::size_t piece : {std::size_t{1}, std::size_t{3}, stream.size()}) {
        Recorder recorder;
        AudioStreamParser parser(recorder);
        push_all(parser, stream, piece);
        EXPECT_EQ(recorder.log, "R1234.500000 A4 A0 R2000.250000 A1 E") << "piece " << piece;
        EXPECT_EQ(recorder.samples, (std::vector<std::int16_t>{1, -2, 32767, -32768, 5}));
        EXPECT_TRUE(parser.ended());
        EXPECT_EQ(parser.pending(), 0u);
    }
}

TEST(AudioStream, ReportsAMessageCutOffAtTheEnd) {
    std::vector<std::byte> stream;
    append_run(stream, 0);
    append_audio(stream, {1, 2, 3});
    Recorder recorder;
    AudioStreamParser parser(recorder);
    parser.push(std::span(stream).first(stream.size() - 1));
    EXPECT_EQ(recorder.log, "R0.000000 ");
    EXPECT_FALSE(parser.ended());
    EXPECT_EQ(parser.pending(), 10u);
}

TEST(AudioStream, RejectsMalformedMessages) {
    const auto rejects = [](const std::vector<std::byte>& stream) {
        Recorder recorder;
        AudioStreamParser parser(recorder);
        EXPECT_THROW(parser.push(stream), InputInvalid);
    };
    std::vector<std::byte> stream;
    append_audio(stream, {1});
    rejects(stream);  // audio before the first run start
    stream.clear();
    append_run(stream, 0);
    append_header(stream, 'A', 3);
    rejects(stream);  // odd audio length
    stream.clear();
    append_header(stream, 'X', 0);
    rejects(stream);
    stream.clear();
    append_header(stream, 'R', 4);
    rejects(stream);
    stream.clear();
    append_header(stream, 'E', 1);
    rejects(stream);
    stream.clear();
    append_run(stream, 0);
    append_header(stream, 'A', AudioStreamParser::max_payload + 2);
    rejects(stream);
    for (const double start : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        stream.clear();
        append_run(stream, start);
        rejects(stream);
    }
    stream.clear();
    append_header(stream, 'E', 0);
    append_run(stream, 0);
    rejects(stream);  // bytes after the end
}
}
