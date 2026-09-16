#include "selftest_log.h"
#include "diagnostic.h"
#include "platform/utf8_paths.h"
#include <fstream>
#include <string>

namespace comskip::output {
void append_selftest_log(std::string_view filename, std::string_view record) {
    std::ofstream output(platform::path_from_utf8(filename), std::ios::binary | std::ios::app);
    if (!output)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(diagnostics::Code::output_open, {std::string(filename)});
    output << record;
    output.close();
    if (!output)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(diagnostics::Code::output_write, {std::string(filename)});
}
}
