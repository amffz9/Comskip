#include "live_xml.h"
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <array>
#include <limits>
#include <sstream>

namespace {
using namespace comskip::output;
TEST(LiveXml, ConvertsPaddedFrameRangesWithSixDecimalTiming) {
    const std::array frames{FrameInterval{12, 245}, FrameInterval{300, 450}};
    std::ostringstream output;
    write_live_dvrmstb(output, frames, 25);
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(output.str().c_str()));
    const auto commercials = document.select_nodes("/root/commercial");
    ASSERT_EQ(commercials.size(), 2u);
    EXPECT_STREQ(commercials[0].node().attribute("start").value(), "0.480000");
    EXPECT_STREQ(commercials[0].node().attribute("end").value(), "9.800000");
    EXPECT_STREQ(commercials[1].node().attribute("start").value(), "12.000000");
    EXPECT_STREQ(commercials[1].node().attribute("end").value(), "18.000000");
}
TEST(LiveXml, RejectsInvalidFrameRateAndRangesBeforeWriting) {
    const std::array frames{FrameInterval{0, 100}};
    for (double fps : {0.0, -25.0, std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity()}) {
        std::ostringstream output;
        EXPECT_THROW(write_live_dvrmstb(output, frames, fps), std::invalid_argument);
        EXPECT_TRUE(output.str().empty());
    }
    for (auto interval : {FrameInterval{30, 20}, FrameInterval{-1, 50}}) {
        std::ostringstream output;
        EXPECT_THROW(write_live_dvrmstb(output, std::span{&interval, 1}, 25), std::invalid_argument);
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
    EXPECT_THROW(write_live_dvrmstb(output, frames, 25), std::ios_base::failure);
}
}
