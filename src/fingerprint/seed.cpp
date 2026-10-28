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
    json::Value document = {{"recording", recording}, {"codePeriod", json::rounded(code_period, 9)}, {"runs", json::Value::array()}};
    for (const auto& run : result.runs) document["runs"].push_back({{"start", json::rounded(run.start, 6)}, {"codes", run.codes}});
    return json::write(document) + "\n";
}

std::string seed_result_json(std::string_view recording, const SeedResult& result) {
    const json::Value document = {{"recording", recording}, {"ended", result.ended}, {"codes", result.codes()},
                                  {"marksFiles", json::Value::array()}, {"promoted", 0}, {"droppedAudioSeconds", 0.0}};
    return json::write(document) + "\n";
}
}
