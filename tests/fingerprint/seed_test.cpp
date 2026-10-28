#include "fingerprint/seed.h"
#include "fingerprint/audio_stream.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
using namespace comskip::fingerprint;

std::vector<std::int16_t> tones(double seconds, double base) {
    std::vector<std::int16_t> samples(static_cast<std::size_t>(seconds * sample_rate));
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double time = static_cast<double>(index) / sample_rate;
        const double frequency = base * (1 + static_cast<int>(time * 3) % 5);
        samples[index] = static_cast<std::int16_t>(9000 * std::sin(6.283185307179586 * frequency * time));
    }
    return samples;
}

void append_header(std::vector<std::byte>& stream, char type, std::uint32_t size) {
    stream.push_back(static_cast<std::byte>(type));
    for (int shift = 0; shift < 32; shift += 8) stream.push_back(static_cast<std::byte>(size >> shift));
}
void append_run(std::vector<std::byte>& stream, double start, const std::vector<std::int16_t>& samples) {
    append_header(stream, 'R', 8);
    const auto bits = std::bit_cast<std::uint64_t>(start);
    for (int shift = 0; shift < 64; shift += 8) stream.push_back(static_cast<std::byte>(bits >> shift));
    // Audio in messages of 1000 samples, as a sender might cut it.
    for (std::size_t offset = 0; offset < samples.size(); offset += 1000) {
        const auto count = std::min<std::size_t>(1000, samples.size() - offset);
        append_header(stream, 'A', static_cast<std::uint32_t>(2 * count));
        for (std::size_t index = offset; index < offset + count; ++index) {
            stream.push_back(static_cast<std::byte>(static_cast<std::uint16_t>(samples[index]) & 0xff));
            stream.push_back(static_cast<std::byte>(static_cast<std::uint16_t>(samples[index]) >> 8));
        }
    }
}

ReadBytes reader(const std::vector<std::byte>& stream, std::size_t piece) {
    return [&stream, piece, offset = std::size_t{0}](std::span<std::byte> buffer) mutable {
        const auto size = std::min({piece, buffer.size(), stream.size() - offset});
        std::memcpy(buffer.data(), stream.data() + offset, size);
        offset += size;
        return size;
    };
}

TEST(Seed, CodesEachRunWithItsOwnCoder) {
    const auto first = tones(12, 220);
    const auto second = tones(2, 330);
    const auto third = tones(9, 440);
    std::vector<std::byte> stream;
    append_run(stream, 0, first);
    append_run(stream, 600.5, second);
    append_run(stream, 1234.5, third);
    append_header(stream, 'E', 0);
    const auto result = seed(reader(stream, 4093));
    ASSERT_EQ(result.runs.size(), 3u);
    EXPECT_TRUE(result.ended);
    EXPECT_EQ(result.runs[0].start, 0.0);
    EXPECT_EQ(result.runs[1].start, 600.5);
    EXPECT_EQ(result.runs[2].start, 1234.5);
    EXPECT_EQ(result.runs[0].codes, code(0, first).codes);
    EXPECT_TRUE(result.runs[1].codes.empty());
    EXPECT_EQ(result.runs[2].codes, code(0, third).codes);
    EXPECT_EQ(result.codes(), static_cast<long long>(result.runs[0].codes.size() + result.runs[2].codes.size()));
}

TEST(Seed, AStreamWithoutItsEndStillGivesItsCodes) {
    const auto samples = tones(6, 220);
    std::vector<std::byte> stream;
    append_run(stream, 10, samples);
    stream.resize(stream.size() - 1);  // the last message is cut off
    const auto result = seed(reader(stream, 1 << 16));
    EXPECT_FALSE(result.ended);
    ASSERT_EQ(result.runs.size(), 1u);
    EXPECT_EQ(result.runs[0].codes, code(10, std::span(samples).first(samples.size() / 1000 * 1000)).codes);
}

TEST(Seed, MalformedStreamIsInputInvalid) {
    std::vector<std::byte> stream;
    append_header(stream, 'A', 2);
    stream.resize(stream.size() + 2);
    EXPECT_THROW(seed(reader(stream, 64)), InputInvalid);
}

TEST(Seed, WritesTheCodeStreamAndPassResult) {
    SeedResult result;
    result.runs = {{0, {1, 4294967295u}}, {1234.5, {}}};
    result.ended = true;
    EXPECT_EQ(code_stream_json("plex:/shows/\"A\"\\b.ts", result),
              R"({"recording":"plex:/shows/\"A\"\\b.ts","codePeriod":0.123809524,"runs":[)"
              R"({"start":0.0,"codes":[1,4294967295]},{"start":1234.5,"codes":[]}]})" "\n");
    EXPECT_EQ(seed_result_json("c1\n", result),
              R"({"recording":"c1\n","ended":true,"codes":2,"marksFiles":[],"promoted":0,"droppedAudioSeconds":0.0})" "\n");
    EXPECT_EQ(code_stream_json("c1", {}), R"({"recording":"c1","codePeriod":0.123809524,"runs":[]})" "\n");
}
}
