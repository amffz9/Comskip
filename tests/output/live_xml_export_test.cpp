#include "recording_context.h"
#include "detection/detector_runtime.h"
#include "detection/frame_causes.h"
#include "checked_format.h"
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <chrono>
#include <filesystem>
#include <memory>

namespace {
std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
class LiveXmlExport : public testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-live-xml-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        auto& settings = context->settings;
        // Decoder startup replaces the default sentinel fps=1 with the actual
        // media rate. This fixture models a decoded 25-fps recording.
        settings.fps = 25;
        settings.output_default = false;
        settings.output_edl = false;
        settings.output_live = false;
        settings.output_dvrmstb = true;
        settings.padding = 2;
        auto& state = context->state;
        comskip::checked_format(state.outbasename, "%s", utf8(directory / "録画 & result").c_str());
        state.framenum_real = 1000;
        state.frame_count = 1001;
        state.frame.resize(1001);
        state.black_count = 3;
        state.black.resize(3);
        state.black[1].frame = 100;
        state.black[1].cause = comskip::detection::cause_value(comskip::detection::FrameCause::black);
        state.black[2].frame = 850;
        state.black[2].cause = comskip::detection::cause_value(comskip::detection::FrameCause::black);
    }
    void TearDown() override {
        context.reset();
        if (!directory.empty()) { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    }
};
TEST_F(LiveXmlExport, RealLiveDetectorWritesXmlWithTheSamePaddingAsTheRecordingList) {
    BuildCommListAsYouGo(*context);
    pugi::xml_document document;
    ASSERT_TRUE(document.load_file((directory / "録画 & result.xml").c_str()));
    ASSERT_EQ(document.select_nodes("/root/commercial").size(), 1u);
    // Two seconds of padding at 25 fps moves each boundary inward by 50 frames.
    EXPECT_STREQ(document.child("root").child("commercial").attribute("start").value(), "6.000000");
    EXPECT_STREQ(document.child("root").child("commercial").attribute("end").value(), "32.000000");
    EXPECT_EQ(context->state.commercial_count, 0);
    EXPECT_EQ(context->state.commercial[0].start_frame, 150);
    EXPECT_EQ(context->state.commercial[0].end_frame, 800);
}
TEST_F(LiveXmlExport, FailedCreationReportsFilenameAndClosesTemporaryResources) {
    context->settings.output_default = true;
    comskip::checked_format(context->state.out_filename, "%s", utf8(directory / "live.txt").c_str());
    comskip::checked_format(context->state.outbasename, "%s", utf8(directory / "missing" / "result").c_str());
    try {
        BuildCommListAsYouGo(*context);
        FAIL() << "Export unexpectedly succeeded";
    } catch (const std::ios_base::failure& error) {
        EXPECT_NE(std::string(error.what()).find("missing"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find(".xml"), std::string::npos);
    }
    EXPECT_EQ(context->state.out_file, nullptr);
    EXPECT_EQ(context->state.edl_file, nullptr);
    EXPECT_EQ(context->state.live_file, nullptr);
}
}
