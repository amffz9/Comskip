#include "output/xml_cutlists.h"
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <array>
#include <limits>
#include <locale>
#include <sstream>

namespace {
using namespace comskip::output;
pugi::xml_document parse(const std::ostringstream& output) {
    pugi::xml_document document;
    EXPECT_TRUE(document.load_string(output.str().c_str(), pugi::parse_default | pugi::parse_declaration));
    EXPECT_STREQ(document.first_child().attribute("encoding").value(), "UTF-8");
    return document;
}
TEST(XmlCutlists, EmptyVideoRedoGolden) {
    std::ostringstream output;
    write_videoredo3(output, {}, {}, {});
    EXPECT_EQ(output.str(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<VideoReDoProject Version=\"3\">\n  <Filename />\n  <CutList />\n  <SceneList />\n</VideoReDoProject>\n");
}
TEST(XmlCutlists, VideoRedoPreservesLongUnicodePathsAndRoundedTimes) {
    const std::string filename = std::string(12000, 'x') + "&<>\"' café 日本語 🎞.ts";
    XmlMediaDescription media{std::filesystem::path(std::u8string(filename.begin(), filename.end())), {}, StreamIds{101, 102, 103}};
    const std::array intervals{TimeInterval{Seconds{0}, Seconds{1.23456785}}};
    const std::array scenes{SceneMarker{Seconds{3600.125}, 9}};
    std::ostringstream output; write_videoredo3(output, media, intervals, scenes);
    auto document = parse(output); auto root = document.child("VideoReDoProject");
    EXPECT_EQ(root.child("Filename").text().as_string(), filename);
    EXPECT_STREQ(root.child("CutList").child("Cut").child("CutTimeStart").text().get(), "0");
    EXPECT_STREQ(root.child("CutList").child("Cut").child("CutTimeEnd").text().get(), "12345679");
    EXPECT_EQ(root.child("CutList").child("InputPIDList").child("AudioStreamPID").text().as_int(), 102);
    EXPECT_STREQ(root.child("SceneList").child("SceneMarker").attribute("Timecode").value(), "1:00:00.12");
}
TEST(XmlCutlists, EdlxPreservesLargeBytePositions) {
    const std::array intervals{ByteInterval{0, 9007199254740993LL}};
    std::ostringstream output; write_edlx(output, intervals); auto document = parse(output);
    auto root = document.child("regionlist");
    EXPECT_STREQ(root.attribute("units").value(), "bytes");
    EXPECT_EQ(root.child("region").attribute("end").as_llong(), intervals[0].end);
}
TEST(XmlCutlists, BtvAndDvrmstbPreserveFullAndZeroLengthIntervals) {
    const std::array intervals{TimeInterval{Seconds{0}, Seconds{12.5}}, TimeInterval{Seconds{12.5}, Seconds{12.5}}};
    std::ostringstream btv; write_btv(btv, intervals); auto b = parse(btv);
    EXPECT_EQ(std::distance(b.child("cutlist").children().begin(), b.child("cutlist").children().end()), 2);
    EXPECT_STREQ(b.child("cutlist").child("Region").child("end").text().get(), "125000000");
    std::ostringstream dvr; write_dvrmstb(dvr, intervals); auto d = parse(dvr);
    EXPECT_STREQ(d.child("root").child("commercial").attribute("start").value(), "0.000000");
    EXPECT_STREQ(d.child("root").child("commercial").attribute("end").value(), "12.500000");
}
TEST(XmlCutlists, CuttermaranEscapesNamesAndParsesConfiguredAttributes) {
    XmlMediaDescription media{{}, std::filesystem::path(u8"é&\"<file>"), {}};
    const std::array retained{FrameInterval{1, 250}};
    std::ostringstream output;
    write_cuttermaran(output, media, retained, {"cut=\"false\" custom=\"A&amp;B\""});
    auto document = parse(output); auto root = document.child("StateData");
    EXPECT_STREQ(root.child("usedVideoFiles").attribute("FileName").value(), "é&\"<file>.M2V");
    EXPECT_EQ(root.child("CutElements").attribute("EndPosition").as_int(), 250);
    EXPECT_STREQ(root.child("CmdArgs").attribute("custom").value(), "A&B");
    EXPECT_STREQ(root.child("CmdArgs").attribute("OutFile").value(), "é&\"<file>_clean.m2v");
    std::ostringstream defaults; write_cuttermaran(defaults, media, {}); auto default_document = parse(defaults);
    EXPECT_TRUE(default_document.child("StateData").child("CmdArgs").attribute("closeApp").as_bool());
}
TEST(XmlCutlists, MkvMergesAdjacentSegmentsAndIncludesFinalShow) {
    const std::array segments{
        ChapterSegment{{Seconds{0}, Seconds{1}}, false},
        ChapterSegment{{Seconds{1}, Seconds{2}}, false},
        ChapterSegment{{Seconds{2}, Seconds{3}}, true},
        ChapterSegment{{Seconds{3}, Seconds{4.125}}, false}};
    MkvOptions options; options.ordered_without_commercials = true;
    options.show_label = "日本語 & Show";
    std::ostringstream output; write_mkv_chapters(output, segments, options); auto document = parse(output);
    EXPECT_EQ(document.select_nodes("/Chapters/EditionEntry[1]/ChapterAtom").size(), 3u);
    EXPECT_EQ(document.select_nodes("/Chapters/EditionEntry[2]/ChapterAtom").size(), 2u);
    auto last = document.select_node("/Chapters/EditionEntry[2]/ChapterAtom[last()]").node();
    EXPECT_STREQ(last.child("ChapterDisplay").child("ChapterString").text().get(), "日本語 & Show");
    EXPECT_STREQ(last.child("ChapterTimeEnd").text().get(), "00:00:04.125000000");
    std::ostringstream empty; write_mkv_chapters(empty, {}, options); EXPECT_TRUE(parse(empty));
}
TEST(XmlCutlists, MkvTagsEscapeEditionTitles) {
    MkvOptions options; options.with_commercials_title = "é & <ads>";
    std::ostringstream output; write_mkv_tags(output, options); auto document = parse(output);
    EXPECT_STREQ(document.child("Tags").child("Tag").child("Simple").child("String").text().get(), "é & <ads>");
    EXPECT_EQ(document.select_nodes("/Tags/Tag").size(), 2u);
}
struct CommaDecimal : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
TEST(XmlCutlists, LocaleIndependentAndDoesNotChangeStreamFormatting) {
    std::ostringstream output; output.imbue(std::locale(std::locale::classic(), new CommaDecimal));
    output.precision(2); auto locale = output.getloc(); auto flags = output.flags();
    const std::array intervals{TimeInterval{Seconds{1.25}, Seconds{2.5}}};
    write_dvrmstb(output, intervals);
    EXPECT_NE(output.str().find("1.250000"), std::string::npos);
    EXPECT_EQ(output.getloc(), locale); EXPECT_EQ(output.precision(), 2); EXPECT_EQ(output.flags(), flags);
}
TEST(XmlCutlists, InvalidInputsDoNotWritePartialDocuments) {
    for (const auto interval : {TimeInterval{Seconds{-1}, Seconds{2}},
                               TimeInterval{Seconds{2}, Seconds{1}},
                               TimeInterval{Seconds{0}, Seconds{std::numeric_limits<double>::infinity()}}}) {
        std::ostringstream output;
        EXPECT_THROW(write_btv(output, std::span{&interval, 1}), std::invalid_argument);
        EXPECT_TRUE(output.str().empty());
    }
    for (const auto* attributes : {"cut=\"x\" cut=\"y\"", "OutFile=\"override\"", "bad", "x=\"v\"><Injected /></CmdArgs><CmdArgs"}) {
        std::ostringstream output;
        EXPECT_THROW(write_cuttermaran(output, {}, {}, {attributes}), std::invalid_argument);
        EXPECT_TRUE(output.str().empty());
    }
    const std::array overlap{ChapterSegment{{Seconds{0}, Seconds{2}}, false}, ChapterSegment{{Seconds{1}, Seconds{3}}, true}};
    std::ostringstream output;
    EXPECT_THROW(write_mkv_chapters(output, overlap), std::invalid_argument);
}
TEST(XmlCutlists, FailedStreamsPropagateErrors) {
    std::ostringstream output; output.setstate(std::ios::badbit);
    EXPECT_THROW(write_edlx(output, {}), std::ios_base::failure);
}
}
