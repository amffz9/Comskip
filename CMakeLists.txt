cmake_minimum_required(VERSION 3.25)
project(Comskip VERSION 0.83.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
find_package(Threads REQUIRED)
option(COMSKIP_BUILD_APP "Build the FFmpeg application" ON)
option(COMSKIP_BUILD_GUI "Build the SDL review interface" OFF)
option(COMSKIP_DONATOR "Enable full-speed decoding" ON)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/defaults.ini" COMSKIP_DEFAULT_INI)
configure_file(cmake/config_defaults.h.in generated/config_defaults.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS config/defaults.ini)
find_package(SimpleIni CONFIG REQUIRED)
add_library(comskip_config src/config/ini.cpp src/config/settings.cpp src/config/profile.cpp)
target_link_libraries(comskip_config PRIVATE SimpleIni::SimpleIni)
target_include_directories(comskip_config PUBLIC src/config PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(READ config/locales/en.ini COMSKIP_ENGLISH_CATALOG)
file(READ config/locales/es.ini COMSKIP_SPANISH_CATALOG)
configure_file(cmake/localization_catalogs.h.in generated/localization_catalogs.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS config/locales/en.ini config/locales/es.ini)
add_library(comskip_localization src/localization/translator.cpp)
target_include_directories(comskip_localization PUBLIC src/localization PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(comskip_localization PUBLIC comskip_config)
add_library(output_text src/output/xml_filename.cpp)
target_include_directories(output_text PUBLIC src/output)

add_library(commercial_length src/detection/commercial_length.cpp)
target_include_directories(commercial_length PUBLIC src/detection)
target_link_libraries(commercial_length PUBLIC comskip_config)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    add_executable(translator_test tests/localization/translator_test.cpp)
    target_link_libraries(translator_test PRIVATE comskip_localization GTest::gtest_main)
    target_compile_definitions(translator_test PRIVATE COMSKIP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    gtest_discover_tests(translator_test)
    add_executable(xml_filename_test tests/output/xml_filename_test.cpp)
    target_link_libraries(xml_filename_test PRIVATE output_text GTest::gtest_main)
    gtest_discover_tests(xml_filename_test)
    add_executable(commercial_length_test tests/detection/commercial_length_test.cpp)
    target_link_libraries(commercial_length_test PRIVATE commercial_length GTest::gtest_main)
    target_compile_definitions(commercial_length_test PRIVATE COMSKIP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    gtest_discover_tests(commercial_length_test TEST_PREFIX default.)
    add_executable(scan_workers_test tests/platform/scan_workers_test.cpp)
    target_include_directories(scan_workers_test PRIVATE src/platform)
    target_link_libraries(scan_workers_test PRIVATE Threads::Threads GTest::gtest_main)
    gtest_discover_tests(scan_workers_test)
    add_executable(settings_test tests/config/settings_test.cpp)
    target_link_libraries(settings_test PRIVATE comskip_config GTest::gtest_main)
    gtest_discover_tests(settings_test)
    add_executable(checked_format_test tests/platform/checked_format_test.cpp)
    target_include_directories(checked_format_test PRIVATE src/app)
    target_link_libraries(checked_format_test PRIVATE GTest::gtest_main)
    gtest_discover_tests(checked_format_test)
    add_executable(platform_test tests/platform/platform_test.cpp src/platform/platform.cpp)
    target_include_directories(platform_test PRIVATE src/platform)
    target_link_libraries(platform_test PRIVATE Threads::Threads GTest::gtest_main)
    gtest_discover_tests(platform_test)
    if(WIN32)
        add_executable(arguments_test tests/platform/arguments_test.cpp)
        target_include_directories(arguments_test PRIVATE src/app)
        target_link_libraries(arguments_test PRIVATE GTest::gtest_main)
        gtest_discover_tests(arguments_test)
    endif()
endif()

if(COMSKIP_BUILD_APP)
    find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h REQUIRED)
    foreach(component avformat avcodec avutil swscale swresample)
        find_library(${component}_LIBRARY NAMES ${component} REQUIRED)
        list(APPEND FFMPEG_LIBRARIES ${${component}_LIBRARY})
    endforeach()
    add_library(media_conversion src/media/audio_samples.cpp src/media/frame_conversion.cpp)
    target_include_directories(media_conversion PUBLIC src/media ${FFMPEG_INCLUDE_DIR})
    target_link_libraries(media_conversion PUBLIC ${swresample_LIBRARY} ${swscale_LIBRARY} ${avutil_LIBRARY})
    if(BUILD_TESTING)
        add_executable(media_conversion_test tests/media/audio_samples_test.cpp tests/media/frame_conversion_test.cpp)
        target_link_libraries(media_conversion_test PRIVATE media_conversion GTest::gtest_main)
        gtest_discover_tests(media_conversion_test)
    endif()
    find_path(ARGTABLE_INCLUDE_DIR argtable2.h REQUIRED)
    find_library(ARGTABLE_LIBRARY NAMES argtable2 REQUIRED)
    add_library(captions STATIC third_party/ccextractor/608.c third_party/ccextractor/ccextractor.c
        third_party/ccextractor/encoding.c third_party/ccextractor/general_loop.c third_party/ccextractor/myth.c)
    target_include_directories(captions PRIVATE src/platform)
    add_executable(comskip 
        src/detection/state.cpp
        src/detection/blocks.cpp
        src/detection/detection.cpp
        src/detection/scoring.cpp
        src/detection/length_matching.cpp
        src/detection/scene_analysis.cpp
        src/detection/logo.cpp
        src/detection/storage.cpp
        src/detection/captions.cpp
        src/detection/live.cpp
        src/ui/review.cpp
        src/config/legacy_settings.cpp
        src/app/runtime.cpp
        src/app/main.cpp
        src/app/csv_input.cpp
        src/output/cutlists.cpp
        src/output/diagnostics.cpp
        src/output/media_dump.cpp
        src/media/frame_timing.cpp
        src/media/mpeg2dec.cpp src/platform/platform.cpp src/ui/video_out_dx.c)
    target_include_directories(comskip PRIVATE src/platform src/ui src/app ${FFMPEG_INCLUDE_DIR} ${ARGTABLE_INCLUDE_DIR})
    target_compile_definitions(comskip PRIVATE PROCESS_CC $<$<BOOL:${COMSKIP_DONATOR}>:DONATOR>)
    target_link_libraries(comskip PRIVATE commercial_length comskip_config comskip_localization output_text media_conversion captions ${FFMPEG_LIBRARIES}
        ${ARGTABLE_LIBRARY} Threads::Threads)
    if(WIN32)
        if(MINGW)
            target_link_options(comskip PRIVATE -municode)
        endif()
        find_library(GETOPT_LIBRARY NAMES getopt REQUIRED)
        target_link_libraries(comskip PRIVATE ${GETOPT_LIBRARY})
        target_link_libraries(comskip PRIVATE comdlg32 gdi32 user32 ddraw dxguid)
        target_compile_definitions(comskip PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_definitions(captions PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_link_libraries(comskip PRIVATE m)
    endif()
    if(COMSKIP_BUILD_GUI)
        find_package(SDL2 REQUIRED)
        target_sources(comskip PRIVATE src/ui/video_out_sdl.c)
        target_compile_definitions(comskip PRIVATE HAVE_SDL)
        target_link_libraries(comskip PRIVATE SDL2::SDL2)
    endif()
    if(BUILD_TESTING)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME media_smoke COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/media/media_smoke.py" $<TARGET_FILE:comskip>
            "${CMAKE_CURRENT_BINARY_DIR}/integration")
        set_tests_properties(media_smoke PROPERTIES TIMEOUT 120)
        find_program(FFMPEG_EXECUTABLE NAMES ffmpeg
            HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/ffmpeg"
            PATH_SUFFIXES tools/ffmpeg)
        if(FFMPEG_EXECUTABLE)
            add_test(NAME media_formats COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/media/media_formats.py" $<TARGET_FILE:comskip>
                "${FFMPEG_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/formats")
            set_tests_properties(media_formats PROPERTIES TIMEOUT 180)
        else()
            message(STATUS "ffmpeg executable unavailable: media_formats test disabled")
        endif()
    endif()
endif()
