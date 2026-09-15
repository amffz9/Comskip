#include "edl.h"

#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <locale>
#include <sstream>

using namespace comskip::output;

TEST(Edl, WritesExactHeaderlessTimesAndAction)
{
    const std::array intervals{CommercialInterval{25, 100}, CommercialInterval{200, 250}};
    std::ostringstream output;
    write_edl(output, intervals, MediaDescription{25}, OutputOptions{0, 3});
    EXPECT_EQ(output.str(), "1.00\t4.00\t3\n8.00\t10.00\t3\n");
}

TEST(Edl, AppliesFrameOffsetsAndClampsBeginning)
{
    const std::array intervals{CommercialInterval{4, 25}, CommercialInterval{25, 100}};
    std::ostringstream output;
    write_edl(output, intervals, MediaDescription{25}, OutputOptions{10, 0});
    EXPECT_EQ(output.str(), "0.00\t0.60\t0\n0.60\t3.60\t0\n");
    output.str("");
    write_edl(output, intervals, MediaDescription{25}, OutputOptions{-25, 0});
    EXPECT_EQ(output.str(), "1.00\t2.00\t0\n2.00\t5.00\t0\n");
}

TEST(Edl, OmitsTinyIntervalsAndSupportsEmptyAndWholeRecording)
{
    std::ostringstream output;
    write_edl(output, {}, MediaDescription{25});
    EXPECT_EQ(output.str(), "");
    const std::array intervals{CommercialInterval{0, 0}, CommercialInterval{10, 12}, CommercialInterval{0, 250}};
    write_edl(output, intervals, MediaDescription{25});
    EXPECT_EQ(output.str(), "0.00\t10.00\t0\n");
}

TEST(Edl, UsesPresentationTimesAndActualFirstFrameCorrection)
{
    const std::array times{Seconds{1.5}, Seconds{1.6}, Seconds{1.8}, Seconds{2.0}, Seconds{2.5}, Seconds{3.0}};
    const MediaDescription media{25, {}, times, 1, Seconds{1.5}};
    const std::array intervals{CommercialInterval{0, 6}};
    std::ostringstream output;
    write_edl(output, intervals, media);
    EXPECT_EQ(output.str(), "1.50\t3.00\t0\n");
    output.str("");
    write_edl(output, intervals, media, OutputOptions{0, 0, true});
    EXPECT_EQ(output.str(), "3.00\t4.50\t0\n");
    output.str("");
    write_edl(output, intervals, media, OutputOptions{100, 0, false, EdlVariant::plus});
    EXPECT_EQ(output.str(), "3.00\t4.50\t0\n");
}

TEST(Edl, PlusAddsFrameTimeAndIgnoresOffsetForConstantFrameRate)
{
    const std::array intervals{CommercialInterval{25, 100}};
    std::ostringstream output;
    write_edl(output, intervals, MediaDescription{25}, OutputOptions{25, 1, false, EdlVariant::plus});
    EXPECT_EQ(output.str(), "1.04\t4.04\t1\n");
}

TEST(Edl, SupportsBoundedTimestampWindowsAndClampsAfterRecordingEnd)
{
    const std::array times{Seconds{0.6}, Seconds{0.8}, Seconds{1.1}, Seconds{1.5}};
    const MediaDescription media{25, {}, times, 15, Seconds{0.04}};
    const std::array intervals{CommercialInterval{25, 28}, CommercialInterval{25, 100}};
    std::ostringstream output;
    write_edl(output, intervals, media, OutputOptions{10});
    EXPECT_EQ(output.str(), "0.60\t1.50\t0\n0.60\t1.50\t0\n");
}

namespace {
class CommaDecimal : public std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};
class FailingBuffer : public std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
};
}

TEST(Edl, IsIndependentOfLocaleAndRetainsStreamFormatting)
{
    std::ostringstream output;
    output.imbue(std::locale(std::locale::classic(), new CommaDecimal));
    output.setf(std::ios::scientific, std::ios::floatfield);
    output.precision(7);
    const auto flags = output.flags();
    const auto locale = output.getloc();
    const std::array intervals{CommercialInterval{25, 100}};
    write_edl(output, intervals, MediaDescription{25});
    EXPECT_EQ(output.str(), "1.00\t4.00\t0\n");
    EXPECT_EQ(output.flags(), flags);
    EXPECT_EQ(output.precision(), 7);
    EXPECT_EQ(output.getloc(), locale);
}

TEST(Edl, ReportsFailedStreamsWithAndWithoutExceptionMask)
{
    const std::array intervals{CommercialInterval{25, 100}};
    FailingBuffer buffer;
    std::ostream output(&buffer);
    EXPECT_THROW(write_edl(output, intervals, MediaDescription{25}), std::ios_base::failure);
    output.clear();
    output.exceptions(std::ios::badbit | std::ios::failbit);
    EXPECT_THROW(write_edl(output, intervals, MediaDescription{25}), std::ios_base::failure);
}

TEST(Edl, RejectsInvalidInputBeforeWriting)
{
    std::ostringstream output;
    const std::array invalid{CommercialInterval{25, 100}, CommercialInterval{10, 5}};
    EXPECT_THROW(write_edl(output, invalid, MediaDescription{25}), std::invalid_argument);
    EXPECT_EQ(output.str(), "");
    EXPECT_THROW(write_edl(output, {}, MediaDescription{0}), std::invalid_argument);
    const std::array invalid_times{Seconds{std::numeric_limits<double>::quiet_NaN()}};
    EXPECT_THROW(write_edl(output, {}, (MediaDescription{25, {}, invalid_times})), std::invalid_argument);
    const std::array overflow{CommercialInterval{10, std::numeric_limits<FrameIndex>::max()}};
    EXPECT_THROW(write_edl(output, overflow, MediaDescription{25}, OutputOptions{-1}), std::out_of_range);
}
