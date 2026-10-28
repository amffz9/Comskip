#include "fingerprint/coder.h"
#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace {
using namespace comskip::fingerprint;

// Chords that change every half second over a little noise, so codes vary.
std::vector<std::int16_t> music(double seconds) {
    std::vector<std::int16_t> samples(static_cast<std::size_t>(seconds * sample_rate));
    std::uint32_t noise = 12345;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto chord = static_cast<int>(index / (sample_rate / 2));
        const double time = static_cast<double>(index) / sample_rate;
        double value = 0;
        for (int voice = 0; voice < 3; ++voice) {
            const double frequency = 110.0 * std::pow(2.0, ((chord * 7 + voice * 4) % 36) / 12.0);
            value += std::sin(2 * std::numbers::pi * frequency * time);
        }
        noise = noise * 1664525u + 1013904223u;
        value = value * 6000 + static_cast<double>(noise >> 20) - 2048;
        samples[index] = static_cast<std::int16_t>(value);
    }
    return samples;
}

long long complete_codes(long long samples) {
    return samples < code_span_samples ? 0 : (samples - code_span_samples) / code_samples + 1;
}

TEST(Coder, CodePeriodIsExactlyChromaprintsHop) {
    EXPECT_NEAR(code_period, 0.123810, 5e-7);
    // The rounded period used by early experiments drifts 3.9 s over 90 minutes.
    EXPECT_NEAR(90 * 60 * (0.1239 / code_period - 1), 3.9, 0.05);
}

TEST(Coder, KeepsTheRunStart) {
    const auto samples = music(5);
    const auto run = code(1234.5, samples);
    EXPECT_EQ(run.start, 1234.5);
    EXPECT_EQ(static_cast<long long>(run.codes.size()), complete_codes(static_cast<long long>(samples.size())));
}

TEST(Coder, ShortRunsGiveFewOrNoCodes) {
    EXPECT_TRUE(code(0, music(2)).codes.empty());
    EXPECT_TRUE(code(0, {}).codes.empty());
    const auto samples = music(4);
    const std::span all(samples);
    EXPECT_TRUE(code(0, all.first(code_span_samples - 1)).codes.empty());
    EXPECT_EQ(code(0, all.first(code_span_samples)).codes.size(), 1u);
    EXPECT_EQ(code(0, all.first(code_span_samples + code_samples)).codes.size(), 2u);
}

TEST(Coder, PiecesGiveTheWholeRunsCodes) {
    const auto samples = music(60);
    const auto whole = code(0, samples).codes;
    ASSERT_EQ(static_cast<long long>(whole.size()), complete_codes(static_cast<long long>(samples.size())));
    for (const std::size_t piece : {std::size_t{sample_rate}, std::size_t{777}, std::size_t{40000}}) {
        Coder coder;
        std::vector<std::uint32_t> streamed;
        for (std::size_t offset = 0; offset < samples.size(); offset += piece) {
            const auto codes = coder.feed(std::span(samples).subspan(offset, std::min(piece, samples.size() - offset)));
            streamed.insert(streamed.end(), codes.begin(), codes.end());
            // Every code whose audio has arrived is returned by the same feed.
            EXPECT_EQ(coder.codes_taken(), complete_codes(coder.samples_fed()));
        }
        EXPECT_EQ(streamed, whole) << "piece " << piece;
    }
}

TEST(Coder, SameInputGivesSameCodes) {
    const auto samples = music(20);
    EXPECT_EQ(code(0, samples).codes, code(7, samples).codes);
}

TEST(Coder, RunsAreIndependent) {
    const auto first = music(10);
    const auto second = music(15);
    Coder coder;
    coder.feed(first);
    Coder fresh;
    EXPECT_EQ(fresh.feed(second), code(0, second).codes);
}
}
