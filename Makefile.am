AM_CPPFLAGS = -I$(srcdir)/src/platform -I$(srcdir)/src/ui -I$(srcdir)/src/app -I$(srcdir)/src/detection
AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = comskip
if BUILD_GUI
    bin_PROGRAMS += comskip-gui
endif
check_PROGRAMS = votest commercial-length-test commercial-length-chinese-test
TESTS = commercial-length-test commercial-length-chinese-test

base_sources = src/detection/comskip.cpp src/app/comskip.h src/detection/commercial_length.cpp src/detection/commercial_length.h src/media/mpeg2dec.cpp src/platform/platform.cpp src/platform/platform.h src/ui/video_out_dx.c src/ui/vo.h \
    third_party/ccextractor/608.c third_party/ccextractor/608.h \
    third_party/ccextractor/ccextractor.c third_party/ccextractor/ccextractor.h \
    third_party/ccextractor/encoding.c \
    third_party/ccextractor/general_loop.c \
    third_party/ccextractor/myth.c

comskip_SOURCES = $(base_sources)
comskip_gui_SOURCES = $(base_sources) src/ui/video_out_sdl.c
votest_SOURCES = src/ui/video_out_dx.c src/ui/video_out_sdl.c
commercial_length_test_SOURCES = tests/commercial_length_test.cpp src/detection/commercial_length.cpp src/detection/commercial_length.h
commercial_length_test_CPPFLAGS = $(AM_CPPFLAGS) $(gtest_CFLAGS)
commercial_length_chinese_test_SOURCES = $(commercial_length_test_SOURCES)
commercial_length_chinese_test_CPPFLAGS = $(AM_CPPFLAGS) $(gtest_CFLAGS) -DCHINESE_SIZE_TABLE

DEFINES = -DPROCESS_CC
if ENABLE_DONATOR
    DEFINES += -DDONATOR
endif

if PLATFORM_WIN32
    PLATFORM_LIBS = -lcomdlg32 -lgdi32
endif

comskip_CPPFLAGS = $(AM_CPPFLAGS) -Wall $(argtable2_CFLAGS) $(ffmpeg_CFLAGS) $(DEFINES)
comskip_LDADD = $(argtable2_LIBS) $(ffmpeg_LIBS) $(PLATFORM_LIBS)
comskip_gui_CPPFLAGS = $(AM_CPPFLAGS) -Wall $(argtable2_CFLAGS) $(ffmpeg_CFLAGS) $(sdl_CFLAGS) $(DEFINES) -DHAVE_SDL
comskip_gui_LDADD = $(argtable2_LIBS) $(ffmpeg_LIBS) $(sdl_LIBS) $(PLATFORM_LIBS)
votest_CPPFLAGS = $(AM_CPPFLAGS) -Wall $(sdl_CFLAGS) -DTEST
votest_LDADD = $(sdl_LIBS) $(PLATFORM_LIBS)

if ENABLE_STATIC
    comskip_LDFLAGS = -static
    comskip_gui_LDFLAGS = -static
endif

if ENABLE_DEBUG
    DEFAULT_CFLAGS = -DDEBUG -ggdb3 -O0
else
    DEFAULT_CFLAGS = -g -O2
endif
comskip_CFLAGS = $(DEFAULT_CFLAGS)
comskip_gui_CFLAGS = $(DEFAULT_CFLAGS)
votest_CFLAGS = $(DEFAULT_CFLAGS)

comskip_CXXFLAGS = $(DEFAULT_CFLAGS) -std=c++23
comskip_gui_CXXFLAGS = $(DEFAULT_CFLAGS) -std=c++23
commercial_length_test_CXXFLAGS = -std=c++23
commercial_length_chinese_test_CXXFLAGS = -std=c++23

commercial_length_test_LDADD = $(gtest_LIBS)
commercial_length_chinese_test_LDADD = $(gtest_LIBS)
