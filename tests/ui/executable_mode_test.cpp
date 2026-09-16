#include "ui/executable_mode.h"
#include "recording_context.h"
#include "exit_requested.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <vector>

int comskip_main(RecordingContext&, int, char**);

TEST(ExecutableMode, PreservesGuiFilenameConventions) {
    EXPECT_TRUE(comskip::ui::gui_executable("comskipGUI.exe"));
    EXPECT_TRUE(comskip::ui::gui_executable("comskip-gui"));
    EXPECT_FALSE(comskip::ui::gui_executable("comskip.exe"));
    EXPECT_FALSE(comskip::ui::gui_executable("build-gui/comskip.exe"));
    EXPECT_FALSE(comskip::ui::gui_executable("GUI/comskip.exe"));
    EXPECT_FALSE(comskip::ui::gui_executable("Café GUI/build-gui/comskip.exe"));
}

TEST(ExecutableMode, GuiDirectoryNamesDoNotEnableInteractiveCliReview) {
    const auto root = std::filesystem::temp_directory_path() /
        ("comskip-mode-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(std::random_device{}()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } cleanup{root};
    ASSERT_TRUE(std::filesystem::create_directory(root));
    const auto media = root / "input.y4m", ini = root / "settings.ini";
    {
        std::ofstream output(media, std::ios::binary);
        output << "YUV4MPEG2 W160 H120 F25:1 Ip A1:1 C420jpeg\n";
        const std::string luma(160 * 120, 80), chroma(160 * 120 / 2, static_cast<char>(128));
        for (int i = 0; i < 150; ++i) output << "FRAME\n" << luma << chroma;
        ASSERT_TRUE(output);
    }
    {
        std::ofstream settings(ini);
        settings << "detect_method=1\noutput_debugwindow=0\nlive_tv_retries=0\nadded_recording=0\nverbose=0\n";
        ASSERT_TRUE(settings);
    }
    for (const char* directory : {"build-gui", "GUI"}) {
        const auto destination = root / directory;
        ASSERT_TRUE(std::filesystem::create_directory(destination));
        auto context = std::make_unique<RecordingContext>();
        std::vector<std::string> args{
            comskip::platform::path_to_utf8(destination / "comskip.exe"),
            "--ini=" + comskip::platform::path_to_utf8(ini),
            "--output=" + comskip::platform::path_to_utf8(destination), "--threads=1",
            comskip::platform::path_to_utf8(media)};
        std::vector<char*> argv;
        for (auto& arg : args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        int status = -1;
        try { status = comskip_main(*context, static_cast<int>(args.size()), argv.data()); }
        catch (const comskip::ExitRequested& exit) { status = exit.status(); }
        EXPECT_TRUE(status == 0 || status == 1);
        EXPECT_FALSE(context->settings.output_debugwindow);
        EXPECT_EQ(context->state.frame_count, 150);
        EXPECT_TRUE(std::filesystem::is_regular_file(destination / "input.txt"));
    }
}
