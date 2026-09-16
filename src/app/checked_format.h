#pragma once
#include <cstdio>
#include <stdexcept>
#include <string>

namespace comskip {
namespace detail {
inline const char* printf_argument(const std::string& value) { return value.c_str(); }
template<class T> const T& printf_argument(const T& value) { return value; }
}
// Reject truncated legacy paths rather than using a different filename.
template<std::size_t Size, class... Args>
void checked_format(char (&destination)[Size], const char* format, Args... args) {
    int count;
    if constexpr (sizeof...(Args) == 0)
        count = std::snprintf(destination, Size, "%s", format);
    else
        count = std::snprintf(destination, Size, format, detail::printf_argument(args)...);
    if (count < 0 || static_cast<std::size_t>(count) >= Size)
        throw std::length_error("Formatted value exceeds the legacy buffer capacity");
}
// Legacy printf callsites can target owned strings without a path-size ceiling.
template<class... Args>
void checked_format(std::string& destination, const char* format, const Args&... args) {
    if constexpr (sizeof...(Args) == 0) {
        destination = format;
    } else {
        const int size = std::snprintf(nullptr, 0, format, detail::printf_argument(args)...);
        if (size < 0) throw std::runtime_error("Could not format value");
        std::string result(static_cast<std::size_t>(size) + 1, '\0');
        if (std::snprintf(result.data(), result.size(), format, detail::printf_argument(args)...) != size)
            throw std::runtime_error("Could not format value");
        result.resize(static_cast<std::size_t>(size));
        destination = std::move(result);
    }
}
}
