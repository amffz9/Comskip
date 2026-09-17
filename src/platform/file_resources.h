#pragma once
#include <cstdio>
#include <memory>
#include <string_view>
#include "platform.h"

namespace comskip::platform {
struct FileCloser {
    void operator()(std::FILE* file) const noexcept {
        if (file) std::fclose(file);
    }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

// Ownership crosses the legacy fopen boundary once, then stays with a value.
inline FilePtr own_file(std::FILE* file) noexcept { return FilePtr{file}; }
inline FilePtr open_file_owned(std::string_view filename, std::string_view mode) noexcept {
    return own_file(open_file(filename, mode));
}
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996) // std::tmpfile is portable; MSVC recommends a Windows-only alternative.
#endif
inline FilePtr temporary_file() noexcept { return own_file(std::tmpfile()); }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}
