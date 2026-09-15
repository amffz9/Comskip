#pragma once
#include <cstdio>
#include <memory>

namespace comskip::platform {
struct FileCloser {
    void operator()(std::FILE* file) const noexcept {
        if (file) std::fclose(file);
    }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

// Ownership crosses the legacy fopen boundary once, then stays with a value.
inline FilePtr own_file(std::FILE* file) noexcept { return FilePtr{file}; }
inline FilePtr temporary_file() noexcept { return own_file(std::tmpfile()); }
}
