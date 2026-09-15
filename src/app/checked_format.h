#pragma once
#include <cstdio>
#include <stdexcept>

namespace comskip {
// Reject truncated legacy paths rather than using a different filename.
template<std::size_t Size, class... Args>
void checked_format(char (&destination)[Size], const char* format, Args... args) {
    int count;
    if constexpr (sizeof...(Args) == 0)
        count = std::snprintf(destination, Size, "%s", format);
    else
        count = std::snprintf(destination, Size, format, args...);
    if (count < 0 || static_cast<std::size_t>(count) >= Size)
        throw std::length_error("Formatted value exceeds the legacy buffer capacity");
}
}
