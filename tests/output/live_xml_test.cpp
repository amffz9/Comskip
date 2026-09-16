#include "live_xml.h"
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <array>
#include <limits>
#include <sstream>

namespace {
using namespace comskip::output;
TEST(LiveXml, PreservesFramePaddingAndSixDecimalTiming) {
    const std::array frames{FrameInterval{12, 245}, FrameInterval{300, 450}};
    std::ostringstream output;
    write_live_dvrmstb(output, frames, 25, 2);
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(output.str().c_str()));
    const auto commercials = document.select_nodes("/root/commercial");
    ASSERT_EQ(commercials.size(), 2u);
    EXPECT_STREQ(commercials[0].node().attribute("start").value(), "0.560000");
    EXPECT_STREQ(commercials[0].node().attribute("end").value(), "9.720000");
    EXPECT_STREQ(commercials[1].node().attribute("start").value(), "12.080000");
    EXPECT_STREQ(commercials[1].node().attribute("end").value(), "17.920000");
}
TEST(LiveXml, AllowsPaddingToExpandACommercialWithoutApplyingFinalExportOffsets) {
    const std::array frames{FrameInterval{25, 100}};
    std::ostringstream output;
    write_live_dvrmstb(output, frames, 25, -5);
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(output.str().c_str()));
    EXPECT_STREQ(document.child("root").child("commercial").attribute("start").value(), "0.800000");
    EXPECT_STREQ(document.child("root").child("commercial").attribute("end").value(), "4.200000");
}
TEST(LiveXml, RejectsInvalidFrameRateAndAdjustedRangesBeforeWriting) {
    const std::array frames{FrameInterval{0, 100}};
    for (double fps : {0.0, -25.0, std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity()}) {
        std::ostringstream output;
        EXPECT_THROW(write_live_dvrmstb(output, frames, fps, 0), std::invalid_argument);
        EXPECT_TRUE(output.str().empty());
    }
    for (auto interval : {FrameInterval{30, 20}, FrameInterval{-1, 50}, FrameInterval{24, 26}}) {
        std::ostringstream output;
        EXPECT_THROW(write_live_dvrmstb(output, std::span{&interval, 1}, 25, 2), std::invalid_argument);
        EXPECT_TRUE(output.str().empty());
    }
}
struct FailedOutput : std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
    int overflow(int) override { return traits_type::eof(); }
};
TEST(LiveXml, ReportsOutputFailure) {
    FailedOutput buffer;
    std::ostream output(&buffer);
    const std::array frames{FrameInterval{25, 100}};
    EXPECT_THROW(write_live_dvrmstb(output, frames, 25, 0), std::ios_base::failure);
}
}
