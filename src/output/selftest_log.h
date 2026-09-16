#pragma once
#include <format>
#include <string_view>
#include <utility>

namespace comskip::output {
void append_selftest_log(std::string_view filename, std::string_view record);
template<class... Args>
void write_selftest_log(std::string_view filename, std::format_string<Args...> format, Args&&... args) {
    append_selftest_log(filename, std::format(format, std::forward<Args>(args)...));
}
}
