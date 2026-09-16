#include "command_line_value.h"
#include <gtest/gtest.h>

using comskip::config::PidParseError;
using comskip::config::parse_transport_stream_pid;

TEST(CommandLineValue, ParsesCompleteHexadecimalTransportStreamPids) {
    EXPECT_EQ(parse_transport_stream_pid("0").value(),0);
    EXPECT_EQ(parse_transport_stream_pid("1a2b").value(),0x1a2b);
    EXPECT_EQ(parse_transport_stream_pid("0X1FFF").value(),0x1fff);
}

TEST(CommandLineValue, RejectsMissingPartialAndOutOfRangePids) {
    EXPECT_EQ(parse_transport_stream_pid("").error(),PidParseError::invalid);
    EXPECT_EQ(parse_transport_stream_pid("0x").error(),PidParseError::invalid);
    EXPECT_EQ(parse_transport_stream_pid("12junk").error(),PidParseError::invalid);
    EXPECT_EQ(parse_transport_stream_pid("xyz").error(),PidParseError::invalid);
    EXPECT_EQ(parse_transport_stream_pid("2000").error(),PidParseError::out_of_range);
    EXPECT_EQ(parse_transport_stream_pid("100000000").error(),PidParseError::out_of_range);
}
