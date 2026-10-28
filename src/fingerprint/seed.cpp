#include "fingerprint/seed.h"
#include "fingerprint/audio_stream.h"
#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace comskip::fingerprint {
namespace {
class SeedEvents final : public AudioStreamEvents {
public:
    explicit SeedEvents(SeedResult& result) : result_(result) {}
    void run_start(double start) override {
        coder_.emplace();
        result_.runs.push_back({start, {}});
    }
    void audio(std::span<const std::int16_t> samples) override {
        auto codes = coder_->feed(samples);
        auto& run = result_.runs.back().codes;
        run.insert(run.end(), codes.begin(), codes.end());
    }
    void end() override { result_.ended = true; }

private:
    SeedResult& result_;
    std::optional<Coder> coder_;
};

void append_string(std::string& text, std::string_view value) {
    text += '"';
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\') {
            text += '\\';
            text += character;
        } else if (byte < 0x20) {
            constexpr char hex[] = "0123456789abcdef";
            text += "\\u00";
            text += hex[byte >> 4];
            text += hex[byte & 0xf];
        } else {
            text += character;
        }
    }
    text += '"';
}

template <class Number> void append_number(std::string& text, Number value, int decimals = 0) {
    std::array<char, 64> buffer{};
    std::to_chars_result result;
    if constexpr (std::is_floating_point_v<Number>) {
        if (!std::isfinite(value)) throw std::invalid_argument("Code stream number is not finite");
        result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, decimals);
    } else {
        result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    }
    if (result.ec != std::errc{}) throw std::invalid_argument("Code stream number cannot be formatted");
    text.append(buffer.data(), result.ptr);
}
}

long long SeedResult::codes() const {
    long long count = 0;
    for (const auto& run : runs) count += static_cast<long long>(run.codes.size());
    return count;
}

SeedResult seed(const ReadBytes& read) {
    SeedResult result;
    SeedEvents events(result);
    AudioStreamParser parser(events);
    std::vector<std::byte> buffer(1 << 16);
    while (const auto size = read(buffer)) parser.push(std::span(buffer).first(size));
    return result;
}

std::string code_stream_json(std::string_view recording, const SeedResult& result) {
    std::string text = "{ \"recording\": ";
    append_string(text, recording);
    text += ", \"codePeriod\": ";
    append_number(text, code_period, 9);
    text += ",\n  \"runs\": [";
    for (std::size_t index = 0; index < result.runs.size(); ++index) {
        const auto& run = result.runs[index];
        text += index ? ",\n    { \"start\": " : "\n    { \"start\": ";
        append_number(text, run.start, 6);
        text += ", \"codes\": [";
        for (std::size_t code = 0; code < run.codes.size(); ++code) {
            if (code) text += ',';
            append_number(text, run.codes[code]);
        }
        text += "] }";
    }
    text += result.runs.empty() ? "] }\n" : " ] }\n";
    return text;
}

std::string seed_result_json(std::string_view recording, const SeedResult& result) {
    std::string text = "{ \"recording\": ";
    append_string(text, recording);
    text += result.ended ? ", \"ended\": true" : ", \"ended\": false";
    text += ", \"codes\": ";
    append_number(text, result.codes());
    text += ", \"marksFiles\": [], \"promoted\": 0, \"droppedAudioSeconds\": 0.0 }\n";
    return text;
}
}
