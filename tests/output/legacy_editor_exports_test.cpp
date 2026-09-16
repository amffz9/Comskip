#include "output/legacy_editor_exports.h"
#include <gtest/gtest.h>
#include <array>
#include <sstream>
using namespace comskip::output;
TEST(LegacyEditorExports, VideoRedo2PreservesLineGrammarTicksPidsAndSceneOrder){
    const std::array cuts{EditorInterval{EditorSeconds{0.125},EditorSeconds{1.25}}};
    const std::array scenes{EditorScene{0,EditorSeconds{2}}};
    std::ostringstream out;write_videoredo2(out,{"Café.ts",true,EditorStreamIds{100,200,300}},cuts,scenes);
    EXPECT_EQ(out.str(),"<Version>2\n<Filename>Café.ts\n<MPEG Stream Type>4\n<VideoStreamPID>100\n<AudioStreamPID>200\n<SubtitlePID1>300\n<Cut>1250000:12500000\n<SceneMarker 0>20000000\n");
}
TEST(LegacyEditorExports, VdrPreservesCentisecondBasedFrameTruncation){
    const std::array cuts{EditorInterval{EditorSeconds{0.125},EditorSeconds{3661.25}}};
    std::ostringstream out;write_vdr(out,cuts,25);
    EXPECT_EQ(out.str(),"0:00:00.03 start\n1:01:01.06 end\n");
}
TEST(LegacyEditorExports, EmptyFormatsPreserveHeaderAndRejectInvalidLateInputBeforeWriting){
    std::ostringstream out;write_videoredo2(out,{"input.ts",false,{}},{},{});
    EXPECT_EQ(out.str(),"<Version>2\n<Filename>input.ts\n");
    std::ostringstream invalid;
    const std::array cuts{EditorInterval{EditorSeconds{0},EditorSeconds{1}},EditorInterval{EditorSeconds{2},EditorSeconds{1}}};
    EXPECT_THROW(write_vdr(invalid,cuts,25),std::invalid_argument);
    EXPECT_THROW(write_videoredo2(invalid,{"input.ts",false,{}},cuts,{}),std::invalid_argument);
    EXPECT_TRUE(invalid.str().empty());
}
TEST(LegacyEditorExports, StreamFailureAndInvalidGeometryPreserveExceptionCategories){
    std::ostringstream out;out.setstate(std::ios::badbit);
    EXPECT_THROW(write_vdr(out,{},25),std::runtime_error);
    EXPECT_THROW(write_videoredo2(out,{"input.ts",false,{}},{},{}),std::runtime_error);
    EXPECT_THROW(write_vdr(out,{},0),std::invalid_argument);
}
