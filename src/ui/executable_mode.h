#pragma once
#include "platform/utf8_paths.h"
#include <string_view>

namespace comskip::ui {
inline bool gui_executable(std::string_view executable_path) {
    const auto filename = comskip::platform::path_to_utf8(
        comskip::platform::path_from_utf8(executable_path).filename());
    return filename.find("GUI") != std::string::npos ||
           filename.find("-gui") != std::string::npos;
}
}
