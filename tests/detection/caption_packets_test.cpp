#include "recording_context.h"
#include "caption_observations.h"
#include "detection_methods.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::unique_ptr<RecordingContext> recording() {
    auto result = std::make_unique<RecordingContext>();
    result->state.initialized = true;
    result->state.framenum = 1;
    result->state.frame.resize(2);
    result->state.cc_text.resize(2);
    result->state.cc_block.resize(2);
    result->settings.verbose = 0;
    return result;
}
void packet(RecordingContext& context, const std::vector<unsigned char>& bytes, int length) {
    std::fill(std::begin(context.state.ccData), std::end(context.state.ccData), 0x41);
    std::copy(bytes.begin(), bytes.end(), context.state.ccData);
    context.state.ccDataLen = length;
    ProcessCCData(context);
}
void xds(RecordingContext& context, unsigned char type, std::string payload, bool valid = true) {
    if (payload.size() % 2) payload += ' ';
    unsigned sum = 1 + type + 15;
    AddXDS(context, 1, type);
    for (std::size_t i = 0; i < payload.size(); i += 2) {
        const auto hi = static_cast<unsigned char>(payload[i]);
        const auto lo = static_cast<unsigned char>(payload[i + 1]);
        sum += hi + lo;
        AddXDS(context, hi, lo);
    }
    AddXDS(context, 0x8f, static_cast<unsigned char>((-sum + !valid) & 0x7f));
}
}

TEST(CaptionPackets, CaptionTypeTextReturnsOwnedLocalizedValues) {
    auto context = recording();
    context->state.processCC = true;
    EXPECT_EQ(CCTypeText(*context, comskip::detection::caption_type_value(comskip::detection::CaptionType::none)), "NONE");
    EXPECT_EQ(CCTypeText(*context, comskip::detection::caption_type_value(comskip::detection::CaptionType::commercial)), "COMMERCIAL");
    EXPECT_EQ(CCTypeText(*context, 73), "73");
    context->state.processCC = false;
    EXPECT_TRUE(CCTypeText(*context, comskip::detection::caption_type_value(comskip::detection::CaptionType::rollup)).empty());
}

TEST(CaptionPackets, EveryTruncatedGa94AndLegacyPacketLeavesObservationsUnchanged) {
    const std::array<std::vector<unsigned char>, 4> packets{{
        {'G','A','9','4',3,0x41,0,0xfc,'H','I'},
        {'C','C',1,0xf8,0x82,0xff,0xc8,0x49,0xff,0x80,0x80},
        {5,2,0,0,0,0,0,4,0,'H','I','J','K'},
        {5,2,0,0,0,0,0,5,0,0,0,0,0,0,4,0,'H','I','J','K'}
    }};
    for (const auto& bytes : packets) for (int length = 0; length < static_cast<int>(bytes.size()); ++length) {
        auto owner = recording();
        owner->state.cc.cc1[0] = 17;
        owner->state.prevccDataLen = 2;
        owner->state.prevccData[0] = 'O';
        owner->state.prevccData[1] = 'K';
        packet(*owner, bytes, length);
        EXPECT_EQ(owner->state.cc.cc1[0], 17) << "length " << length;
        EXPECT_EQ(owner->state.cc_text[0].text_len, 0);
        EXPECT_EQ(owner->state.prevccDataLen, 2);
        EXPECT_EQ(owner->state.prevccData[0], 'O');
    }
}
TEST(CaptionPackets, RejectsInvalidFlagsAndOversizedDeclaredLengthsThenAcceptsValidText) {
    auto owner = recording();
    packet(*owner, {'G','A','9','4',3,0x41,0,0xf8,'N','O'}, 10);
    EXPECT_EQ(owner->state.cc_text[0].text_len, 0);
    packet(*owner, {'G','A','9','4',3,0x41,0,0xfc,'H','I'}, 501);
    EXPECT_EQ(owner->state.cc_text[0].text_len, 0);
    packet(*owner, {'G','A','9','4',3,0x41,0,0xfc,'H','I'}, 10);
    EXPECT_EQ(owner->state.cc_text[0].text_len, 2);
    EXPECT_STREQ(reinterpret_cast<const char*>(owner->state.cc_text[0].text), "HI");
}
TEST(CaptionPackets, ControlOnlyPairsOnEmptyTextPreserveFollowingPrintableText) {
    auto owner = recording();
    for (unsigned char control : {0x21, 0x22, 0x23, 0x24}) {
        owner->state.cc.cc1[0] = 0x14;
        owner->state.cc.cc1[1] = control;
        AddCC(*owner, 0);
        EXPECT_EQ(owner->state.cc_text_count, 0);
        EXPECT_EQ(owner->state.cc_text[0].text_len, 0);
        EXPECT_EQ(owner->state.cc_text[0].text[0], 0);
    }
    owner->state.cc.cc1[0] = 'H';
    owner->state.cc.cc1[1] = 'I';
    AddCC(*owner, 0);
    EXPECT_STREQ(reinterpret_cast<const char*>(owner->state.cc_text[0].text), "HI");
}

TEST(CaptionPackets, FirstBlockDiagnosticDoesNotReadBeforeOwnedStorage) {
    auto owner = recording();
    owner->state.cc_block[0].start_frame = 10;
    owner->state.cc_block[0].end_frame = 20;
    owner->state.cc_block[0].type = comskip::detection::caption_type_value(comskip::detection::CaptionType::popon);
    EXPECT_NO_THROW(OutputCCBlock(*owner, 0));
    EXPECT_NO_THROW(OutputCCBlock(*owner, -1));
}

TEST(CaptionPackets, ExtendedCharactersSurviveTextSplittingAndRemainTerminated) {
    auto owner = recording();
    for (int pair = 0; pair < 150; ++pair) {
        owner->state.cc.cc1[0] = '*'; // The basic CEA-608 map stores this as 0xe1.
        owner->state.cc.cc1[1] = '*';
        AddCC(*owner, 0);
    }
    std::vector<unsigned char> observed;
    for (long index = 0; index <= owner->state.cc_text_count; ++index) {
        const auto& row = owner->state.cc_text[index];
        ASSERT_GE(row.text_len, 0);
        ASSERT_LT(row.text_len, static_cast<long>(std::size(row.text)));
        EXPECT_EQ(row.text[row.text_len], 0);
        observed.insert(observed.end(), row.text, row.text + row.text_len);
    }
    EXPECT_GT(owner->state.cc_text_count, 0);
    EXPECT_EQ(observed, std::vector<unsigned char>(300, 0xe1));
}

TEST(CaptionPackets, DictionarySearchIsCaseInsensitiveWithoutMutatingCaptionText) {
    auto owner = recording();
    const auto path = std::filesystem::temp_directory_path() /
        ("comskip-caption-dictionary-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    {
        std::ofstream dictionary(path);
        ASSERT_TRUE(dictionary);
        dictionary << "-----\r\nOFFER\r\n";
    }
    const auto path_text = path.u8string();
    owner->state.dictfilename.assign(reinterpret_cast<const char*>(path_text.data()), path_text.size());
    owner->state.cc_text_count = 1;
    owner->state.cc_text[0].start_frame = 1;
    owner->state.cc_text[0].end_frame = 2;
    owner->state.block_count = 1;
    owner->state.cblock[0].f_start = 0;
    owner->state.cblock[0].f_end = 3;
    owner->state.cblock[0].score = 10.0;
    const std::string original = "special offer";
    std::copy(original.begin(), original.end(), owner->state.cc_text[0].text);
    owner->state.cc_text[0].text_len = static_cast<long>(original.size());

    EXPECT_TRUE(ProcessCCDict(*owner));
    EXPECT_STREQ(reinterpret_cast<const char*>(owner->state.cc_text[0].text), original.c_str());
    EXPECT_NE(owner->state.cblock[0].score, 10.0);

    std::error_code error;
    EXPECT_TRUE(std::filesystem::remove(path, error)) << error.message();
}

TEST(XdsPackets, FirstValidTitleIsObservedAndBadChecksumCannotReplaceIt) {
    auto owner = recording();
    xds(*owner, 3, "ORIGINAL");
    EXPECT_STREQ(owner->state.XDS_block[owner->state.XDS_block_count].name, "ORIGINAL");
    const auto count = owner->state.XDS_block_count;
    xds(*owner, 3, "CORRUPTED", false);
    EXPECT_EQ(owner->state.XDS_block_count, count);
    EXPECT_STREQ(owner->state.XDS_block[count].name, "ORIGINAL");
    xds(*owner, 3, "NEXT");
    EXPECT_STREQ(owner->state.XDS_block[owner->state.XDS_block_count].name, "NEXT");
}
TEST(XdsPackets, LongPacketsAndMoreThanFortyTypesPreserveSubsequentMetadata) {
    auto owner = recording();
    xds(*owner, 3, std::string(200, 'L'));
    const auto& long_name = owner->state.XDS_block[owner->state.XDS_block_count].name;
    EXPECT_EQ(std::string_view(long_name), std::string(39, 'L'));
    EXPECT_EQ(long_name[39], '\0');
    xds(*owner, 3, std::string(200, 'M'));
    EXPECT_EQ(owner->state.XDS_block[owner->state.XDS_block_count].name[0], 'M');
    for (unsigned char type = 16; type < 80; ++type) xds(*owner, type, "DATA");
    EXPECT_LE(owner->state.lastXDS, 40);
    xds(*owner, 3, "FINAL");
    EXPECT_STREQ(owner->state.XDS_block[owner->state.XDS_block_count].name, "FINAL ");
}
TEST(XdsPackets, ShortFieldsAndOverflowedAssemblyCannotReusePreviousPayload) {
    auto owner = recording();
    xds(*owner, 2, "ABCD");
    const auto count = owner->state.XDS_block_count;
    const auto duration = owner->state.XDS_block[count].duration;
    xds(*owner, 2, "");
    EXPECT_EQ(owner->state.XDS_block_count, count);
    EXPECT_EQ(owner->state.XDS_block[count].duration, duration);
    const auto position = owner->state.XDS_block[count].position;
    xds(*owner, 2, "EF"); // Elapsed-position bytes are optional.
    EXPECT_EQ(owner->state.XDS_block[owner->state.XDS_block_count].duration, ('F' << 8) + 'E');
    EXPECT_EQ(owner->state.XDS_block[owner->state.XDS_block_count].position, position);
    xds(*owner, 3, std::string(1100, 'X'));
    EXPECT_TRUE(owner->state.startXDS);
    xds(*owner, 3, "RECOVERED");
    EXPECT_STREQ(owner->state.XDS_block[owner->state.XDS_block_count].name, "RECOVERED ");
}
