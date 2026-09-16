#include "run_log.h"
#include "localization/diagnostic.h"
#include "platform/utf8_paths.h"
#include <fstream>
#include <ios>
#include <string>

namespace comskip::output {
void write_run_footer(std::string_view path, std::string_view timestamp) {
    std::ofstream output(comskip::platform::path_from_utf8(path),std::ios::binary|std::ios::app);
    if (!output)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(
            diagnostics::Code::output_open,{std::string(path)});
    output << "################################################################\n"
           << "Time at end of run:\n" << timestamp
           << "################################################################\n";
    output.close();
    if (!output)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(
            diagnostics::Code::output_write,{std::string(path)});
}
}
