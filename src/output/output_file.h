#pragma once

#include "diagnostic.h"
#include "platform/utf8_paths.h"
#include <chrono>
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

} // namespace comskip::output
