#include "xml_filename.h"

#include <gtest/gtest.h>

TEST(XmlFilename, EscapesElementTextAndPreservesHistoricalPercentRepresentation)
{
    EXPECT_EQ(comskip::output::escape_xml_filename("a&b<c>d%\"'.ts"),
              "a&amp;b&lt;c&gt;d&#37;\"'.ts");
    EXPECT_EQ(comskip::output::escape_xml_filename(""), "");
}

TEST(XmlFilename, HandlesLongNamesWithExpansionWithoutFixedBuffer)
{
    const std::string filename(4096, '&');
    const auto escaped = comskip::output::escape_xml_filename(filename);
    ASSERT_EQ(escaped.size(), filename.size() * 5);
    for (std::size_t offset = 0; offset < escaped.size(); offset += 5)
        EXPECT_EQ(escaped.substr(offset, 5), "&amp;");
}

TEST(XmlFilename, PreservesUnicodeAndEscapesCompleteDirectoryAndFilename)
{
    const auto bytes = std::u8string(u8"録画 café & archive/episode <1>.ts");
    const std::string filename(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    const auto expected_bytes = std::u8string(u8"録画 café &amp; archive/episode &lt;1&gt;.ts");
    const std::string expected(reinterpret_cast<const char*>(expected_bytes.data()), expected_bytes.size());
    EXPECT_EQ(comskip::output::escape_xml_filename(filename), expected);
}

TEST(XmlFilename, ReturnedValuesHaveIndependentOwnership)
{
    const auto first = comskip::output::escape_xml_filename("first&.ts");
    const auto second = comskip::output::escape_xml_filename("second<.ts");
    EXPECT_EQ(first, "first&amp;.ts");
    EXPECT_EQ(second, "second&lt;.ts");
}
