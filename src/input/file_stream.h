#include "../localization/diagnostic.h"
#pragma once
#include <cstdio>
#include <istream>
#include <stdexcept>
#include <streambuf>
#include <array>

namespace comskip::input {
// Nonowning adapter for an already-open C file. Its FilePtr owner must outlive
// the stream; stream errors remain visible to read_text_line.
class FileStreamBuffer final : public std::streambuf {
    FILE* file_;
    std::array<char, 4096> bytes_{};
    int_type underflow() override {
        const auto count = std::fread(bytes_.data(), 1, bytes_.size(), file_);
        if (!count) {
            if (std::ferror(file_)) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_read_input_file);
            return traits_type::eof();
        }
        setg(bytes_.data(), bytes_.data(), bytes_.data() + count);
        return traits_type::to_int_type(*gptr());
    }
public:
    explicit FileStreamBuffer(FILE& file) noexcept : file_(&file) {}
};
}
