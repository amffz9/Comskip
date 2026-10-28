#include "recording_context.h"
#include "detection/detector_runtime.h"
#include "detection/frame_causes.h"
#include "output/evidence_adapter.h"
#include "checked_format.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>

namespace {
using comskip::detection::FrameCause;
using comskip::detection::cause_value;

std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
std::string read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, {}};
}
using Json = nlohmann::json;
// The whole file as JSON, or null when it is not one complete document.
Json parsed(const std::string& text) { return Json::parse(text, nullptr, false); }
long long revision(const std::string& text) {
    const auto document = parsed(text);
    return document.is_object() && document.contains("revision") ? document["revision"].get<long long>() : -1;
}
class EvidenceExport : public testing::Test {
protected:
    std::filesystem::path directory;
    std::filesystem::path evidence;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-evidence-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        evidence = directory / "録画.evidence.json";
        context = std::make_unique<RecordingContext>();
        auto& settings = context->settings;
        settings.fps = 25;
        settings.output_default = false;
        settings.output_edl = true;
        settings.output_live = false;
        settings.output_evidence = true;
        settings.padding = 2;
        auto& state = context->state;
        comskip::checked_format(state.outbasename, "%s", utf8(directory / "録画").c_str());
        state.framenum_real = 1000;
        state.framesprocessed = 1000;
        state.frame_count = 1000;
        state.frame.resize(1001);
        for (std::size_t i = 0; i < state.frame.size(); ++i) state.frame[i].pts = static_cast<double>(i) / 25;
        state.black_count = 3;
        state.black.resize(3);
        state.black[1].frame = 100;
        state.black[1].cause = cause_value(FrameCause::black);
        state.black[2].frame = 850;
        state.black[2].cause = cause_value(FrameCause::black) | cause_value(FrameCause::silence);
    }
    void TearDown() override {
        context.reset();
        if (!directory.empty()) { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    }
};

TEST_F(EvidenceExport, LiveRewritesBesideTheEdlWithAGrowingRevision) {
    BuildCommListAsYouGo(*context);
    const auto first = read(evidence);
    EXPECT_EQ(revision(first), 1);
    const auto document = parsed(first);
    EXPECT_EQ(document["final"], false);
    EXPECT_NE(std::ranges::find(document["seams"], Json{{"at", 34.0}, {"kinds", {"black", "silence"}}}), document["seams"].end());
    // The padded live interval, as the EDL lists it; live intervals have no score.
    EXPECT_NE(std::ranges::find(document["blocks"], Json{{"from", 6.0}, {"to", 32.0}, {"score", nullptr}, {"decision", "commercial"}}),
              document["blocks"].end());
    EXPECT_TRUE(std::filesystem::exists(directory / "録画.edl"));
    EXPECT_FALSE(std::filesystem::exists(directory / "録画.evidence.json.tmp"));
    context->state.lastFrameCommCalculated = 0;
    BuildCommListAsYouGo(*context);
    EXPECT_EQ(revision(read(evidence)), 2);
}

TEST_F(EvidenceExport, OffByDefaultWritesNothing) {
    context->settings.output_evidence = false;
    BuildCommListAsYouGo(*context);
    EXPECT_TRUE(std::filesystem::exists(directory / "録画.edl"));
    EXPECT_FALSE(std::filesystem::exists(evidence));
}

TEST_F(EvidenceExport, FinalListsScoredBlocksAndLogoAndDropsTheEndCut) {
    auto& state = context->state;
    state.logoInfoAvailable = true;
    context->settings.commDetectMethod = 255;
    for (long i = 200; i < 400; ++i) state.frame[i].logo_present = true;
    state.black_count = 4;
    state.black.resize(4);
    state.black[3].frame = 1000;  // the cut block building adds at the end
    state.black[3].cause = cause_value(FrameCause::black);
    state.block_count = 2;
    state.cblock.resize(2);
    state.cblock[0] = {}; state.cblock[0].f_start = 1; state.cblock[0].f_end = 100; state.cblock[0].score = 0.5;
    state.cblock[1] = {}; state.cblock[1].f_start = 100; state.cblock[1].f_end = 850; state.cblock[1].score = 1.25;
    state.commercial_count = 0;
    state.commercial.resize(1);
    state.commercial[0] = {100, 850, 1, 1, 30.0};
    state.evidence_revision = 4;
    WriteEvidenceFile(*context, true);
    EXPECT_EQ(parsed(read(evidence)), Json::parse(R"({ "revision": 5, "final": true, "logoKnown": true,
        "seams": [ { "at": 4.0, "kinds": ["black"] }, { "at": 34.0, "kinds": ["black", "silence"] } ],
        "logo": [ { "from": 8.0, "to": 16.0 } ],
        "blocks": [ { "from": 0.04, "to": 4.0, "score": 0.5, "decision": "programme" },
                    { "from": 4.0, "to": 34.0, "score": 1.25, "decision": "commercial" } ] })"));
}

TEST_F(EvidenceExport, ReadersNeverSeeAHalfWrittenFileDuringLiveRewrites) {
    BuildCommListAsYouGo(*context);
    std::atomic<bool> done{false};
    std::atomic<int> partial{0}, reads{0};
    std::atomic<long long> last_seen{0};
    std::thread reader([&] {
        while (!done) {
            const auto text = read(evidence);
            if (text.empty()) continue;  // between opening and a replace on some systems
            ++reads;
            const auto seen = revision(text);
            if (seen < 0 || !text.ends_with("}\n") || seen < last_seen)
                ++partial;
            last_seen = seen;
        }
    });
    for (int i = 0; i < 100; ++i) {
        context->state.lastFrameCommCalculated = 0;
        BuildCommListAsYouGo(*context);
    }
    done = true;
    reader.join();
    EXPECT_EQ(partial, 0);
    EXPECT_GT(reads, 0);
    // Without a reader the next rewrite always lands.
    context->state.lastFrameCommCalculated = 0;
    BuildCommListAsYouGo(*context);
    EXPECT_EQ(revision(read(evidence)), 102);
}
}
