#include "recording_context.h"
#include "output/xml_output_adapter.h"
#include "checked_format.h"

#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <chrono>
#include <filesystem>
#include <memory>
#include <random>

namespace {
std::string utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
class XmlExport : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-xml-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        auto& state = context->state; auto& settings = context->settings;
        comskip::checked_format(state.outbasename, "%s", utf8(directory / "result").c_str());
        comskip::checked_format(state.inbasename, "%s", "décode & <input>");
        comskip::checked_format(state.mpegfilename, "%s", utf8(directory / std::filesystem::path(u8"é & <input>.ts")).c_str());
        settings.fps = 25; settings.videoredo_offset = 1;
        settings.output_videoredo3 = true; settings.output_videoredo = true;
        settings.output_edlx = true; settings.output_btv = true;
        settings.output_cuttermaran = true; settings.output_dvrmstb = true; settings.output_mkvtoolnix = 2;
        state.frame_count = 12; state.framenum_real = 12; state.frame.resize(12);
        for (int i = 0; i < 12; ++i) { state.frame[i].pts = i / 25.0; state.frame[i].goppos = i * 100; }
        state.commercial_count = 0; state.commercial[0].start_frame = 4; state.commercial[0].end_frame = 9;
        state.reffer_count = 0; state.reffer[0].start_frame = 6; state.reffer[0].end_frame = 10;
        state.block_count = 3;
        state.cblock[0].f_start = 1; state.cblock[0].f_end = 3; state.cblock[0].iscommercial = false;
        state.cblock[1].f_start = 4; state.cblock[1].f_end = 9; state.cblock[1].iscommercial = true;
        state.cblock[2].f_start = 10; state.cblock[2].f_end = 11; state.cblock[2].iscommercial = false;
    }
    void TearDown() override {
        context.reset();
        if (!directory.empty()) { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    }
    pugi::xml_document load(const char* extension) {
        auto filename = directory / "result"; filename += extension;
        pugi::xml_document document;
        EXPECT_TRUE(document.load_file(filename.c_str()));
        return document;
    }
};
TEST_F(XmlExport, NormalExportWritesAllFormatsWithResolvedOffsets) {
    WriteXmlOutputFiles(*context);
    auto project = load(".VPrj"); auto root = project.child("VideoReDoProject");
    EXPECT_EQ(root.child("Filename").text().as_string(), std::string(context->state.mpegfilename));
    EXPECT_STREQ(root.child("CutList").child("Cut").child("CutTimeStart").text().get(), "800000");
    EXPECT_STREQ(root.child("CutList").child("Cut").child("CutTimeEnd").text().get(), "2800000");
    EXPECT_EQ(project.select_nodes("/VideoReDoProject/SceneList/SceneMarker").size(), 3u);
    auto bytes = load(".edlx"); EXPECT_EQ(bytes.child("regionlist").child("region").attribute("start").as_int(), 400);
    EXPECT_EQ(bytes.child("regionlist").child("region").attribute("end").as_int(), 900);
    auto btv = load(".chapters.xml"); EXPECT_EQ(btv.select_nodes("/cutlist/Region").size(), 1u);
    EXPECT_STREQ(btv.child("cutlist").child("Region").child("end").text().get(), "3600000");
    auto cuttermaran = load(".cpf"); EXPECT_STREQ(cuttermaran.child("StateData").child("usedVideoFiles").attribute("FileName").value(), "décode & <input>.M2V");
    auto dvr = load(".xml"); EXPECT_STREQ(dvr.child("root").child("commercial").attribute("start").value(), "0.160000");
    auto chapters = load(".mkvtoolnix.chapters");
    EXPECT_EQ(chapters.select_nodes("/Chapters/EditionEntry[1]/ChapterAtom").size(), 3u);
    EXPECT_EQ(chapters.select_nodes("/Chapters/EditionEntry[2]/ChapterAtom").size(), 2u);
    EXPECT_STREQ(chapters.select_node("/Chapters/EditionEntry[2]/ChapterAtom[last()]/ChapterTimeEnd").node().text().get(), "00:00:00.440000000");
    EXPECT_EQ(load(".mkvtoolnix.tags").select_nodes("/Tags/Tag").size(), 2u);
}
TEST_F(XmlExport, ReviewExportUsesReferenceMarksAndReplacesCompleteDocuments) {
    WriteXmlOutputFiles(*context); WriteXmlOutputFiles(*context, true);
    auto project = load(".VPrj");
    EXPECT_EQ(project.select_nodes("/VideoReDoProject").size(), 1u);
    EXPECT_STREQ(project.select_node("/VideoReDoProject/CutList/Cut/CutTimeStart").node().text().get(), "1600000");
    auto bytes = load(".edlx"); EXPECT_EQ(bytes.child("regionlist").child("region").attribute("start").as_int(), 600);
    auto chapters = load(".mkvtoolnix.chapters");
    EXPECT_STREQ(chapters.select_node("/Chapters/EditionEntry[1]/ChapterAtom[2]/ChapterTimeStart").node().text().get(), "00:00:00.240000000");
    context->state.reffer_count = -1; WriteXmlOutputFiles(*context, true);
    EXPECT_EQ(load(".VPrj").select_nodes("/VideoReDoProject/CutList/Cut").size(), 0u);
    EXPECT_EQ(load(".chapters.xml").select_nodes("/cutlist/Region").size(), 0u);
    EXPECT_EQ(load(".mkvtoolnix.chapters").select_nodes("/Chapters/EditionEntry[1]/ChapterAtom").size(), 1u);
}
TEST_F(XmlExport, FullRecordingCommercialHasNoOrderedShowChapters) {
    context->state.reffer[0].start_frame = 0; context->state.reffer[0].end_frame = 11;
    WriteXmlOutputFiles(*context, true);
    EXPECT_EQ(load(".VPrj").select_nodes("/VideoReDoProject/CutList/Cut").size(), 1u);
    EXPECT_EQ(load(".mkvtoolnix.chapters").select_nodes("/Chapters/EditionEntry[2]/ChapterAtom").size(), 0u);
}
}
