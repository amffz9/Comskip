#include "recording_context.h"
#include "detection/storage.h"
#include "detection/image_geometry.h"
#include "detection/buffer_growth.h"
#include "detection/interval_storage.h"
#include "detection/detection_blocks.h"
#include "detection/logo_shrink.h"
#include "detection/reference_comparison.h"
#include "diagnostic_render.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>
namespace {
using namespace comskip::detection;
using namespace comskip::diagnostics;
template<class Exception, class Action>
void failure(Action action, Code code, std::string_view english, std::string_view spanish) {
    try { action(); FAIL() << "Accepted invalid geometry or storage"; }
    catch (const Exception& error) {
        const auto* provider=dynamic_cast<const DiagnosticProvider*>(&error);
        ASSERT_NE(provider,nullptr); EXPECT_EQ(provider->diagnostic().code,code);
        EXPECT_TRUE(provider->diagnostic().arguments.empty());
        EXPECT_NE(comskip::localization::render_exception(error,comskip::localization::Translator("en")).find(english),std::string::npos);
        EXPECT_NE(comskip::localization::render_exception(error,comskip::localization::Translator("es")).find(spanish),std::string::npos);
    }
}
}
TEST(GeometryDiagnostics, ImageValidationPreservesInvalidAndLengthCategories) {
    failure<std::invalid_argument>([]{checked_image_size(0,120);},Code::image_dimensions_and_channel_count_must_be_positive,
        "must be positive","deben ser positivos");
    failure<std::length_error>([]{checked_image_size(2,2,std::numeric_limits<std::size_t>::max());},
        Code::image_dimensions_exceed_the_addressable_buffer_size,"addressable buffer size","búfer direccionable");
}
TEST(GeometryDiagnostics, BufferGrowthRejectsInvalidIndexWithoutChangingOwnedData) {
    std::vector<int> buffer{17}; long capacity=1;
    failure<std::out_of_range>([&]{grow_buffer(buffer,capacity,-1,10);},
        Code::invalid_detection_buffer_index_or_capacity,"Invalid detection buffer","búfer de detección no válidos");
    EXPECT_EQ(buffer,(std::vector<int>{17})); EXPECT_EQ(capacity,1);
}
TEST(GeometryDiagnostics, IntervalAndBlockValidationRetainCompletedStorage) {
    std::vector<Legacy_commercial_entry> intervals; int last=0;
    failure<std::out_of_range>([&]{validate_intervals(intervals,last);},
        Code::interval_count_does_not_match_owned_storage,"does not match owned storage","almacenamiento propio");
    std::vector<block_info> blocks{empty_block()}; long count=0;
    failure<std::out_of_range>([&]{erase_blocks(blocks,count,0,1);},Code::invalid_detection_block_removal,
        "Invalid detection block removal","Eliminación de bloque de detección no válida");
    EXPECT_EQ(count,0); EXPECT_EQ(blocks.size(),1u); EXPECT_EQ(blocks[0].score,1);
}
TEST(GeometryDiagnostics, LogoArithmeticKeepsInvalidArgumentAndRangeCategories) {
    failure<std::invalid_argument>([]{logo_shrink(1e20,0,25);},
        Code::logo_shrink_must_fit_a_nonnegative_frame_offset,"nonnegative frame offset","fotograma no negativo");
    failure<std::out_of_range>([]{add_logo_frames(std::numeric_limits<int>::max(),1);},
        Code::logo_shrink_arithmetic_exceeds_the_frame_index_type,"frame index type","índice de fotograma");
}
TEST(GeometryDiagnostics, ActualBlockInitializerRejectsBeforePublishingObservation) {
    auto context=std::make_unique<RecordingContext>(); context->state.cblock={empty_block()};
    context->state.cblock[0].score=2;
    failure<std::out_of_range>([&]{InitializeBlockArray(*context,1);},
        Code::detection_block_initialization_exceeds_owned_storage,"exceeds owned storage","almacenamiento propio");
    EXPECT_EQ(context->state.cblock[0].score,2);
}
TEST(GeometryDiagnostics, EveryActualProducerRejectsNegativeIndexBeforeGrowthOrInitialization) {
    using Initialize=void(*)(RecordingContext&,long);
    for (Initialize initialize : {InitializeFrameArray,InitializeBlackArray,InitializeSchangeArray,
            InitializeLogoBlockArray,InitializeARBlockArray,InitializeACBlockArray,InitializeBlockArray,
            InitializeCCBlockArray,InitializeCCTextArray}) {
        auto context=std::make_unique<RecordingContext>();
        context->state.frame.resize(1); context->state.frame[0].brightness=17;
        context->state.black.resize(1); context->state.black[0].brightness=18;
        context->state.schange.resize(1); context->state.schange[0].percentage=19;
        context->state.logo_block.resize(1); context->state.logo_block[0].start=20;
        context->state.ar_block.resize(1); context->state.ar_block[0].start=21;
        context->state.ac_block.resize(1); context->state.ac_block[0].start=22;
        context->state.cblock={empty_block()}; context->state.cblock[0].score=23;
        context->state.cc_block.resize(1); context->state.cc_block[0].start_frame=24;
        context->state.cc_text.resize(1); context->state.cc_text[0].text_len=25;
        failure<std::out_of_range>([&]{initialize(*context,-1);},Code::invalid_detection_buffer_index_or_capacity,
            "Invalid detection buffer index","búfer de detección no válidos");
        EXPECT_EQ(context->state.frame.size(),1u); EXPECT_EQ(context->state.frame[0].brightness,17);
        EXPECT_EQ(context->state.black.size(),1u); EXPECT_EQ(context->state.black[0].brightness,18);
        EXPECT_EQ(context->state.schange.size(),1u); EXPECT_EQ(context->state.schange[0].percentage,19);
        EXPECT_EQ(context->state.logo_block[0].start,20); EXPECT_EQ(context->state.ar_block[0].start,21);
        EXPECT_EQ(context->state.ac_block[0].start,22); EXPECT_EQ(context->state.cblock[0].score,23);
        EXPECT_EQ(context->state.cc_block[0].start_frame,24); EXPECT_EQ(context->state.cc_text[0].text_len,25);
        EXPECT_EQ(context->state.max_frame_count,0); EXPECT_EQ(context->state.max_black_count,0);
    }
}
TEST(GeometryDiagnostics, ReferenceComparisonUsesLocalizedValidation) {
    failure<std::invalid_argument>([]{compare_reference_intervals({}, {},-1);},
        Code::negative_reference_comparison_tolerance,"Negative reference comparison tolerance","comparación de referencia negativa");
}
