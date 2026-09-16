#include "recording_context.h"
#include "output/xml_output_adapter.h"
#include "checked_format.h"

#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
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
        state.mpegfilename = utf8(directory / std::filesystem::path(u8"é & <input>.ts"));
        settings.fps = 25; settings.videoredo_offset = 1;
        settings.output_videoredo3 = true; settings.output_videoredo = true;
        settings.output_edlx = true; settings.output_btv = true;
        settings.output_cuttermaran = true; settings.output_dvrmstb = true; settings.output_mkvtoolnix = 2;
        settings.output_plist_cutlist = true;
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
TEST_F(XmlExport, PlistPreservesPresentationTimesAndTailSentinelForNormalAndReview) {
    WriteXmlOutputFiles(*context);
    auto normal = load(".plist");
    auto integers = normal.select_nodes("/array/integer");
    ASSERT_EQ(integers.size(), 4u);
    EXPECT_EQ(integers[0].node().text().as_llong(), 14400);
    EXPECT_EQ(integers[1].node().text().as_llong(), 32400);
    EXPECT_EQ(integers[2].node().text().as_llong(), 36000);
    EXPECT_EQ(integers[3].node().text().as_llong(), 39600);
    WriteXmlOutputFiles(*context, true);
    auto review = load(".plist");
    integers = review.select_nodes("/array/integer");
    ASSERT_EQ(integers.size(), 2u);
    EXPECT_EQ(integers[0].node().text().as_llong(), 21600);
    EXPECT_EQ(integers[1].node().text().as_llong(), 36000);
    context->state.reffer_count = -1;
    WriteXmlOutputFiles(*context, true);
    auto empty_marks = load(".plist");
    integers = empty_marks.select_nodes("/array/integer");
    ASSERT_EQ(integers.size(), 2u);
    EXPECT_EQ(integers[0].node().text().as_llong(), 36000);
    EXPECT_EQ(integers[1].node().text().as_llong(), 39600);
}
TEST_F(XmlExport, PlistOnlyExportHandlesShortMarksAndClampsTerminalBoundary) {
    auto& settings = context->settings;
    settings.output_videoredo3 = settings.output_edlx = settings.output_btv = false;
    settings.output_cuttermaran = settings.output_dvrmstb = false;
    settings.output_mkvtoolnix = 0;
    context->state.commercial[0].start_frame = 0;
    context->state.commercial[0].end_frame = 1;
    WriteXmlOutputFiles(*context);
    auto document = load(".plist");
    auto integers = document.select_nodes("/array/integer");
    ASSERT_EQ(integers.size(), 4u);
    // get_frame_pts historically clamps frame zero to the first stored PTS.
    EXPECT_EQ(integers[0].node().text().as_llong(), 3600);
    EXPECT_EQ(integers[1].node().text().as_llong(), 3600);
    context->state.commercial[0].end_frame = context->state.frame_count;
    WriteXmlOutputFiles(*context);
    auto terminal = load(".plist");
    integers = terminal.select_nodes("/array/integer");
    ASSERT_EQ(integers.size(), 2u);
    EXPECT_EQ(integers[1].node().text().as_llong(), 39600);
}
TEST_F(XmlExport, FullRecordingCommercialHasNoOrderedShowChapters) {
    context->state.reffer[0].start_frame = 0; context->state.reffer[0].end_frame = 11;
    WriteXmlOutputFiles(*context, true);
    EXPECT_EQ(load(".VPrj").select_nodes("/VideoReDoProject/CutList/Cut").size(), 1u);
    EXPECT_EQ(load(".mkvtoolnix.chapters").select_nodes("/Chapters/EditionEntry[2]/ChapterAtom").size(), 0u);
}
TEST_F(XmlExport, SpanishExportLocalizesChapterLabelsAndEditionTitles) {
    context->translator = comskip::localization::Translator("es");
    WriteXmlOutputFiles(*context);
    auto chapters = load(".mkvtoolnix.chapters");
    EXPECT_STREQ(chapters.select_node("/Chapters/EditionEntry[1]/ChapterAtom[1]/ChapterDisplay/ChapterString").node().text().get(), "Programa");
    EXPECT_STREQ(chapters.select_node("/Chapters/EditionEntry[1]/ChapterAtom[2]/ChapterDisplay/ChapterString").node().text().get(), "Anuncio");
    auto tags = load(".mkvtoolnix.tags");
    EXPECT_STREQ(tags.select_node("/Tags/Tag[1]/Simple/String").node().text().get(), "Con anuncios");
    EXPECT_STREQ(tags.select_node("/Tags/Tag[2]/Simple/String").node().text().get(), "Sin anuncios");
}
TEST_F(XmlExport, MissingSelectedLabelsFallBackToEnglishDuringExport) {
    using comskip::config::Ini;
    context->translator = comskip::localization::Translator("es",
        Ini("output_commercial=Commercial\noutput_show=Show\noutput_with_commercials=With Commercials\noutput_without_commercials=Without Commercials\n"),
        Ini{});
    WriteXmlOutputFiles(*context);
    auto chapters = load(".mkvtoolnix.chapters");
    auto tags = load(".mkvtoolnix.tags");
    EXPECT_STREQ(chapters.select_node("/Chapters/EditionEntry[1]/ChapterAtom[1]/ChapterDisplay/ChapterString").node().text().get(), "Show");
    EXPECT_STREQ(tags.select_node("/Tags/Tag[2]/Simple/String").node().text().get(), "Without Commercials");
}
TEST_F(XmlExport, InvalidCountsGeometryAndRangesFailBeforeWriting) {
    const auto expect_unchanged = [&] {
        auto filename = directory / "result.VPrj";
        { std::ofstream file(filename); file << "previous output"; }
        EXPECT_ANY_THROW(WriteXmlOutputFiles(*context));
        std::ifstream file(filename);
        EXPECT_EQ(std::string(std::istreambuf_iterator<char>(file), {}), "previous output");
        EXPECT_FALSE(std::filesystem::exists(directory / "result.edlx"));
    };
    context->state.block_count = 1001; expect_unchanged(); context->state.block_count = 3;
    context->state.block_count = -1; expect_unchanged(); context->state.block_count = 3;
    context->state.commercial_count = 100000; expect_unchanged(); context->state.commercial_count = 0;
    context->state.frame_count = 13; expect_unchanged(); context->state.frame_count = 12;
    context->state.framenum_real = 13; expect_unchanged(); context->state.framenum_real = 12;
    context->settings.fps = 0; expect_unchanged(); context->settings.fps = 25;
    context->state.commercial[0].end_frame = 13; expect_unchanged(); context->state.commercial[0].end_frame = 9;
    context->state.commercial_count = 1;
    context->state.commercial[1].start_frame = 8; context->state.commercial[1].end_frame = 10;
    expect_unchanged(); context->state.commercial_count = 0;
    context->state.cblock[2].f_start = 8; expect_unchanged(); context->state.cblock[2].f_start = 10;
    context->settings.cuttermaran_options = "invalid attributes"; expect_unchanged();
}
TEST_F(XmlExport, TerminalDetectorBoundaryClampsToLastStoredFrame) {
    // The real decoder produces a terminal inclusive block end equal to count.
    // There is deliberately no extra frame slot to disguise an out-of-bounds read.
    context->state.commercial[0].end_frame = context->state.frame_count;
    context->state.cblock[1].f_end = context->state.frame_count;
    context->state.block_count = 2;
    context->settings.videoredo_offset = 0;
    ASSERT_EQ(context->state.frame.size(), static_cast<std::size_t>(context->state.frame_count));
    WriteXmlOutputFiles(*context);
    auto project = load(".VPrj");
    EXPECT_STREQ(project.select_node("/VideoReDoProject/CutList/Cut/CutTimeEnd").node().text().get(), "4400000");
    auto bytes = load(".edlx");
    EXPECT_EQ(bytes.child("regionlist").child("region").attribute("end").as_int(), 1100);
    auto dvr = load(".xml");
    EXPECT_STREQ(dvr.child("root").child("commercial").attribute("end").value(), "0.440000");
    auto chapters = load(".mkvtoolnix.chapters");
    EXPECT_EQ(chapters.select_nodes("/Chapters/EditionEntry[1]/ChapterAtom").size(), 2u);
    // A boundary cannot start a new interval; no corresponding media frame exists.
    context->state.commercial[0].start_frame = context->state.frame_count;
    EXPECT_THROW(WriteXmlOutputFiles(*context), std::out_of_range);
}
}
