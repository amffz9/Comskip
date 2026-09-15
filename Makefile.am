AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = comskip
if BUILD_GUI
    bin_PROGRAMS += comskip-gui
endif
check_PROGRAMS = votest commercial-length-test commercial-length-chinese-test
TESTS = commercial-length-test commercial-length-chinese-test

base_sources = comskip.c comskip.h commercial_length.c commercial_length.h mpeg2dec.c platform.c platform.h video_out_dx.c vo.h \
    ccextratorwin/608.c ccextratorwin/608.h \
    ccextratorwin/ccextractor.c ccextratorwin/ccextractor.h \
    ccextratorwin/encoding.c \
    ccextratorwin/general_loop.c \
    ccextratorwin/myth.c

comskip_SOURCES = $(base_sources)
comskip_gui_SOURCES = $(base_sources) video_out_sdl.c
votest_SOURCES = video_out_dx.c video_out_sdl.c
commercial_length_test_SOURCES = tests/commercial_length_test.c commercial_length.c commercial_length.h
commercial_length_test_CPPFLAGS = -I$(srcdir)
commercial_length_chinese_test_SOURCES = $(commercial_length_test_SOURCES)
commercial_length_chinese_test_CPPFLAGS = -I$(srcdir) -DCHINESE_SIZE_TABLE

DEFINES = -DPROCESS_CC
if ENABLE_DONATOR
    DEFINES += -DDONATOR
endif

if PLATFORM_WIN32
    PLATFORM_LIBS = -lcomdlg32 -lgdi32
endif

comskip_CPPFLAGS = -Wall $(argtable2_CFLAGS) $(ffmpeg_CFLAGS) $(DEFINES)
comskip_LDADD = $(argtable2_LIBS) $(ffmpeg_LIBS) $(PLATFORM_LIBS)
comskip_gui_CPPFLAGS = -Wall $(argtable2_CFLAGS) $(ffmpeg_CFLAGS) $(sdl_CFLAGS) $(DEFINES) -DHAVE_SDL
comskip_gui_LDADD = $(argtable2_LIBS) $(ffmpeg_LIBS) $(sdl_LIBS) $(PLATFORM_LIBS)
votest_CPPFLAGS = -Wall $(sdl_CFLAGS) -DTEST
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
