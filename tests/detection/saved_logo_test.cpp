#include "recording_context.h"
#include "detection/saved_logo.h"
#include <gtest/gtest.h>
#include <fstream>
#include <chrono>
#include <memory>
void LoadLogoMaskData(RecordingContext&);
void SaveLogoMaskData(RecordingContext&);
namespace {
struct Fixture {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        std::filesystem::u8path("saved-logo-café-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".logo");
    std::unique_ptr<RecordingContext> context = std::make_unique<RecordingContext>();
    Fixture() {
        auto& state=context->state;
        state.width=state.videowidth=160; state.height=120;
        state.ensure_pixel_buffers(true);
        state.clogoMinX=10; state.clogoMaxX=11; state.clogoMinY=10; state.clogoMaxY=11;
        state.choriz_edgemask[1610]=7; state.cvert_edgemask[1610]=8;
        state.loadingCSV=true;
        const auto utf8=path.u8string(); state.logofilename=std::string(reinterpret_cast<const char*>(utf8.data()),utf8.size());
        context->settings.verbose=0; context->settings.output_default=false;
        context->settings.startOverAfterLogoInfoAvail=false;
        context->settings.edge_radius=2; context->settings.edge_step=1; context->settings.border=0;
    }
    ~Fixture() { std::error_code error; std::filesystem::remove(path,error); }
    void write(std::string metadata, std::string masks="\x80\n| \n |\n\x81\n- \n -\n") {
        std::ofstream file(path,std::ios::binary); file << metadata << masks;
    }
    void unchanged() {
        EXPECT_EQ(context->state.width,160); EXPECT_EQ(context->state.height,120);
        EXPECT_EQ(context->state.clogoMinX,10); EXPECT_EQ(context->state.clogoMaxX,11);
        EXPECT_EQ(context->state.choriz_edgemask[1610],7);
        EXPECT_EQ(context->state.cvert_edgemask[1610],8);
        EXPECT_FALSE(context->state.logoInfoAvailable);
        EXPECT_FALSE(context->settings.startOverAfterLogoInfoAvail);
        // On Windows this proves the exception released the opened stream.
        EXPECT_TRUE(std::filesystem::remove(path));
    }
};
}
TEST(SavedLogo, ActualFilePreservesFractionalTruncationAndLoadsBothMasks) {
    Fixture fixture;
    fixture.write("picWidth=160.9\npicHeight=120.9\nlogoMinX=10.9\nlogoMaxX=11.9\nlogoMinY=10.9\nlogoMaxY=11.9\n");
    EXPECT_NO_THROW(LoadLogoMaskData(*fixture.context));
    EXPECT_EQ(fixture.context->state.width,160); EXPECT_EQ(fixture.context->state.clogoMinX,10);
    EXPECT_EQ(fixture.context->state.choriz_edgemask[1610],1);
    EXPECT_EQ(fixture.context->state.choriz_edgemask[1611],0);
    EXPECT_EQ(fixture.context->state.cvert_edgemask[1771],1);
    EXPECT_TRUE(fixture.context->state.logoInfoAvailable);
    EXPECT_TRUE(std::filesystem::remove(fixture.path));
}
TEST(SavedLogo, MissingMetadataUsesExistingGeometryAndCombinedMaskOverrides) {
    Fixture fixture;
    fixture.write("Saved logo\n", "\x80\n| \n |\n\x81\n- \n -\n\x82\n+|\n- \n");
    EXPECT_NO_THROW(LoadLogoMaskData(*fixture.context));
    EXPECT_EQ(fixture.context->state.choriz_edgemask[1610],1);
    EXPECT_EQ(fixture.context->state.cvert_edgemask[1610],1);
    EXPECT_EQ(fixture.context->state.choriz_edgemask[1770],0);
    EXPECT_EQ(fixture.context->state.cvert_edgemask[1770],1);
}
TEST(SavedLogo, ActualWriterCombinedOnlyFileRoundTripsWithoutLosingGeometryOrMasks) {
    Fixture fixture;
    fixture.context->state.choriz_edgemask[1610]=1;
    fixture.context->state.cvert_edgemask[1610]=1;
    fixture.context->state.choriz_edgemask[1611]=1;
    fixture.context->state.cvert_edgemask[1611]=0;
    EXPECT_NO_THROW(SaveLogoMaskData(*fixture.context));
    fixture.context->state.choriz_edgemask[1610]=0;
    fixture.context->state.cvert_edgemask[1610]=0;
    EXPECT_NO_THROW(LoadLogoMaskData(*fixture.context));
    EXPECT_EQ(fixture.context->state.choriz_edgemask[1610],1);
    EXPECT_EQ(fixture.context->state.cvert_edgemask[1610],1);
    EXPECT_EQ(fixture.context->state.choriz_edgemask[1611],1);
    EXPECT_EQ(fixture.context->state.cvert_edgemask[1611],0);
    EXPECT_EQ(fixture.context->state.clogoMinX,10);
}
TEST(SavedLogo, EveryExtremeOrMalformedMetadataFieldRejectsBeforePublishing) {
    for (const char* key : {"picWidth","picHeight","logoMinX","logoMaxX","logoMinY","logoMaxY"})
        for (const char* value : {"1e20","nan","inf","oops","-1"}) {
            SCOPED_TRACE(std::string(key)+"="+value);
            Fixture fixture; fixture.write(std::string(key)+"="+value+"\n");
            EXPECT_THROW(LoadLogoMaskData(*fixture.context),std::invalid_argument);
            fixture.unchanged();
        }
}
TEST(SavedLogo, InvalidGeometryAndTruncatedMasksPreservePriorDimensionsAndBytes) {
    for (const std::string metadata : {"picWidth=0\n", "logoMaxX=160\n", "logoMinY=12\n"}) {
        Fixture fixture; fixture.write(metadata);
        EXPECT_THROW(LoadLogoMaskData(*fixture.context),std::invalid_argument); fixture.unchanged();
    }
    for (const std::string mask : {"", "\x80\n|", "\x80\n| \n |\n", "\x80\n??\n??\n", "\x80\n| \n |\n\x81\n-", "\x82\n+"}) {
        Fixture fixture; fixture.write("picWidth=320\n",mask);
        EXPECT_THROW(LoadLogoMaskData(*fixture.context),std::invalid_argument); fixture.unchanged();
    }
}
