#include "recording_context.h"
#include "detection/storage.h"
#include "detection/image_geometry.h"
#include "detection/buffer_growth.h"
#include "detection/interval_storage.h"
#include "detection/detection_blocks.h"
#include "detection/logo_shrink.h"
#include "detection/logo_detection.h"
#include "detection/reference_comparison.h"
#include "diagnostic_render.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <ranges>
void Add_XDS_block(RecordingContext&);
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
TEST(GeometryDiagnostics, FrameInitializationClearsReusedStateAndCarriesOnlyXds) {
    auto context=std::make_unique<RecordingContext>();
    context->state.frame.resize(2);
    context->state.max_frame_count=2;
    context->state.frame[0].xds=42;
    context->state.frame[0].brightness=17;
    context->state.frame[1]=frame_info{
        .brightness=99, .schange_percent=3, .volume=88, .commercial=true,
        .goppos=77, .logo_filter=6.5, .xds=1, .audio_channels=7,
    };

    InitializeFrameArray(*context,1);

    const auto& initialized=context->state.frame[1];
    EXPECT_EQ(initialized.schange_percent,100);
    EXPECT_EQ(initialized.xds,42);
    EXPECT_EQ(initialized.ar_ratio,0.0);
    EXPECT_EQ(initialized.audio_channels,0);
    EXPECT_EQ(initialized.brightness,0);
    EXPECT_EQ(initialized.volume,0);
    EXPECT_FALSE(initialized.commercial);
    EXPECT_EQ(initialized.goppos,0);
    EXPECT_EQ(initialized.logo_filter,0.0);
    EXPECT_EQ(context->state.frame[0].brightness,17);
}
TEST(GeometryDiagnostics, StorageInitializersResetReusedRecordsAndPreserveNeighbors) {
    auto context=std::make_unique<RecordingContext>();
    context->state.curvolume=31;
    context->state.black.resize(2); context->state.max_black_count=2;
    context->state.black[0].brightness=12; context->state.black[1]={9,8,7,6,5};
    context->state.schange.resize(2); context->state.max_schange_count=2;
    context->state.schange[0].percentage=12; context->state.schange[1]={8,7};
    context->state.logo_block.resize(2); context->state.max_logo_block_count=2;
    context->state.logo_block[0]={1,2}; context->state.logo_block[1]={3,4};
    context->state.ar_block.resize(2); context->state.max_ar_block_count=2;
    context->state.ar_block[0].start=1; context->state.ar_block[1].start=3;
    context->state.ac_block.resize(2); context->state.max_ac_block_count=2;
    context->state.ac_block[0].start=1; context->state.ac_block[1].start=3;
    context->state.cc_block.resize(2); context->state.max_cc_block_count=2;
    context->state.cc_block[0].start_frame=1; context->state.cc_block[1]={3,4,5};
    context->state.cc_text.resize(2); context->state.max_cc_text_count=2;
    context->state.cc_text[0].text_len=1; context->state.cc_text[1].text_len=9;
    context->state.cc_text[1].text[0]='X';

    InitializeBlackArray(*context,1); InitializeSchangeArray(*context,1);
    InitializeLogoBlockArray(*context,1); InitializeARBlockArray(*context,1);
    InitializeACBlockArray(*context,1); InitializeCCBlockArray(*context,1);
    InitializeCCTextArray(*context,1);

    EXPECT_EQ(context->state.black[1].brightness,255); EXPECT_EQ(context->state.black[1].volume,31);
    EXPECT_EQ(context->state.black[1].frame,0); EXPECT_EQ(context->state.black[1].cause,0);
    EXPECT_EQ(context->state.schange[1].frame,1); EXPECT_EQ(context->state.schange[1].percentage,100);
    EXPECT_EQ(context->state.logo_block[1].start,0); EXPECT_EQ(context->state.logo_block[1].end,0);
    EXPECT_EQ(context->state.ar_block[1].start,0); EXPECT_EQ(context->state.ac_block[1].start,0);
    EXPECT_EQ(context->state.cc_block[1].start_frame,-1); EXPECT_EQ(context->state.cc_block[1].end_frame,-1);
    EXPECT_EQ(context->state.cc_block[1].type,0); EXPECT_EQ(context->state.cc_text[1].text_len,0);
    EXPECT_EQ(context->state.cc_text[1].text[0],0);
    EXPECT_EQ(context->state.black[0].brightness,12); EXPECT_EQ(context->state.schange[0].percentage,12);
    EXPECT_EQ(context->state.logo_block[0].start,1); EXPECT_EQ(context->state.ar_block[0].start,1);
    EXPECT_EQ(context->state.ac_block[0].start,1); EXPECT_EQ(context->state.cc_block[0].start_frame,1);
    EXPECT_EQ(context->state.cc_text[0].text_len,1);
}
TEST(GeometryDiagnostics, ActualStorageGrowthPublishesCapacityAndValueInitializedSpareRecords) {
    auto context=std::make_unique<RecordingContext>();
    InitializeLogoBlockArray(*context,21);

    EXPECT_EQ(context->state.max_logo_block_count,40);
    ASSERT_EQ(context->state.logo_block.size(),42u);
    EXPECT_TRUE(std::ranges::all_of(context->state.logo_block, [](const logo_block_info& record) {
        return record.start==0 && record.end==0;
    }));
}
TEST(GeometryDiagnostics, ReferenceComparisonUsesLocalizedValidation) {
    failure<std::invalid_argument>([]{compare_reference_intervals({}, {},-1);},
        Code::negative_reference_comparison_tolerance,"Negative reference comparison tolerance","comparación de referencia negativa");
}
TEST(GeometryDiagnostics, ActualLogoNullPixelsRejectBeforeWritingEdgeState) {
    auto context=std::make_unique<RecordingContext>();
    context->state.width=context->state.videowidth=160; context->state.height=120;
    context->settings.edge_radius=2; context->settings.edge_step=1; context->settings.border=0;
    context->state.ensure_pixel_buffers(true); context->state.hor_edgecount[0]=17;
    failure<std::invalid_argument>([&]{EdgeDetect(*context,{},0);},
        Code::logo_edge_detection_requires_image_pixels,"Logo edge detection requires image pixels","bordes del logotipo requiere píxeles");
    EXPECT_EQ(context->state.hor_edgecount[0],17);
}
TEST(GeometryDiagnostics, ActualXdsIndexRejectsBeforeAddingObservation) {
    auto context=std::make_unique<RecordingContext>();
    context->state.frame.resize(1); context->state.frame[0].xds=17;
    context->state.XDS_block.resize(1); context->state.XDS_block_count=-1;
    failure<std::out_of_range>([&]{Add_XDS_block(*context);},
        Code::invalid_xds_block_index,"Invalid XDS block index","Índice de bloque XDS no válido");
    EXPECT_EQ(context->state.frame[0].xds,17); EXPECT_EQ(context->state.XDS_block_count,-1);
    EXPECT_EQ(context->state.XDS_block.size(),1u);
}
