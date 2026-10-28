#include "fingerprint/seed.h"
#include "fingerprint/audio_stream.h"
#include "fingerprint/json.h"
#include <optional>

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
    json::append_string(text, recording);
    text += ", \"codePeriod\": ";
    json::append_number(text, code_period, 9);
    text += ",\n  \"runs\": [";
    for (std::size_t index = 0; index < result.runs.size(); ++index) {
        const auto& run = result.runs[index];
        text += index ? ",\n    { \"start\": " : "\n    { \"start\": ";
        json::append_number(text, run.start, 6);
        text += ", \"codes\": [";
        for (std::size_t code = 0; code < run.codes.size(); ++code) {
            if (code) text += ',';
            json::append_integer(text, run.codes[code]);
        }
        text += "] }";
    }
    text += result.runs.empty() ? "] }\n" : " ] }\n";
    return text;
}

std::string seed_result_json(std::string_view recording, const SeedResult& result) {
    std::string text = "{ \"recording\": ";
    json::append_string(text, recording);
    text += result.ended ? ", \"ended\": true" : ", \"ended\": false";
    text += ", \"codes\": ";
    json::append_integer(text, result.codes());
    text += ", \"marksFiles\": [], \"promoted\": 0, \"droppedAudioSeconds\": 0.0 }\n";
    return text;
}
}
