#pragma once

#include "diagnostic.h"
#include "platform/utf8_paths.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>

namespace comskip::output {

inline void write_output_file(const std::string& filename, std::string_view contents,
                              std::chrono::milliseconds retry_delay = {})
{
    const auto path = comskip::platform::path_from_utf8(filename);
    std::ofstream output(path, std::ios::binary);
    if (!output && retry_delay > std::chrono::milliseconds::zero()) {
        std::this_thread::sleep_for(retry_delay);
        output.clear();
        output.open(path, std::ios::binary);
    }
    if (!output)
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_open, {filename});

    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output)
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_write, {filename});
}

// Writes a sibling ".tmp" file and renames it over filename, so a reader sees
// the previous contents or the new ones, never a partial file. A rename that
// fails, as on Windows while a reader holds the file, is retried up to five
// times when retry_delay is positive.
inline void replace_output_file(const std::string& filename, std::string_view contents,
                                std::chrono::milliseconds retry_delay = {})
{
    const auto path = comskip::platform::path_from_utf8(filename);
    auto temporary = path;
    temporary += ".tmp";
    write_output_file(comskip::platform::path_to_utf8(temporary), contents, retry_delay);
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    for (int attempt = 0; error && attempt < 5 && retry_delay > std::chrono::milliseconds::zero(); ++attempt) {
        std::this_thread::sleep_for(retry_delay);
        std::filesystem::rename(temporary, path, error);
    }
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_write, {filename});
    }
}

} // namespace comskip::output
