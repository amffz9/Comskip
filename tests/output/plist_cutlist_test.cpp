#include "output/plist_cutlist.h"
#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
using namespace comskip::output;
TEST(PlistCutlist, PreservesFragmentAndNinetyThousandTickGolden) {
    const std::array intervals{TimeInterval{Seconds{1.25}, Seconds{2.5}},
                              TimeInterval{Seconds{3}, Seconds{3}}};
    std::ostringstream output;
    write_plist_cutlist(output, intervals);
    EXPECT_EQ(output.str(), "<array>\n<integer>112500</integer> <integer>225000</integer>\n"
                            "<integer>270000</integer> <integer>270000</integer>\n</array>\n");
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(output.str().c_str()));
    EXPECT_FALSE(document.child("plist"));
    EXPECT_EQ(document.select_nodes("/array/integer").size(), 4u);
}
TEST(PlistCutlist, EmptyArrayRemainsAFragment) {
    std::ostringstream output;
    write_plist_cutlist(output, {});
    EXPECT_EQ(output.str(), "<array>\n</array>\n");
}
struct GroupedNumbers : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '.'; }
    std::string do_grouping() const override { return "\3"; }
};
TEST(PlistCutlist, TruncatesFractionalTicksAndIgnoresStreamLocaleAndFlags) {
    std::ostringstream output;
    output.imbue(std::locale(std::locale::classic(), new GroupedNumbers));
    output << std::hex << std::showbase << std::setprecision(2);
    const auto flags = output.flags();
    const auto locale = output.getloc();
    const std::array intervals{TimeInterval{Seconds{1.9 / 90000}, Seconds{2.9 / 90000}}};
    write_plist_cutlist(output, intervals);
    EXPECT_EQ(output.str(), "<array>\n<integer>1</integer> <integer>2</integer>\n</array>\n");
    EXPECT_EQ(output.flags(), flags);
    EXPECT_EQ(output.precision(), 2);
    EXPECT_EQ(output.getloc(), locale);
}
TEST(PlistCutlist, ValidatesAllIntervalsBeforeWritingAndChecksIntegerBounds) {
    const double limit = std::ldexp(1.0, 63) / 90000;
    for (const auto interval : {TimeInterval{Seconds{-1}, Seconds{1}},
                               TimeInterval{Seconds{2}, Seconds{1}},
                               TimeInterval{Seconds{0}, Seconds{std::numeric_limits<double>::quiet_NaN()}},
                               TimeInterval{Seconds{0}, Seconds{std::numeric_limits<double>::infinity()}}}) {
        std::ostringstream output;
        const std::array intervals{TimeInterval{Seconds{0}, Seconds{1}}, interval};
        EXPECT_THROW(write_plist_cutlist(output, intervals), std::exception);
        EXPECT_TRUE(output.str().empty());
    }
    std::ostringstream rejected;
    const std::array excessive{TimeInterval{Seconds{0}, Seconds{limit}}};
    EXPECT_THROW(write_plist_cutlist(rejected, excessive), std::out_of_range);
    EXPECT_TRUE(rejected.str().empty());
    std::ostringstream accepted;
    const std::array large{TimeInterval{Seconds{0}, Seconds{std::nextafter(limit, 0.0)}}};
    EXPECT_NO_THROW(write_plist_cutlist(accepted, large));
    pugi::xml_document document;
    ASSERT_TRUE(document.load_string(accepted.str().c_str()));
    EXPECT_GT(document.child("array").last_child().text().as_llong(), 0);
}
struct FailedOutput : std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
    int overflow(int) override { return traits_type::eof(); }
};
TEST(PlistCutlist, ReportsStreamFailure) {
    FailedOutput buffer;
    std::ostream output(&buffer);
    const std::array intervals{TimeInterval{Seconds{0}, Seconds{1}}};
    EXPECT_THROW(write_plist_cutlist(output, intervals), std::ios_base::failure);
}
}
