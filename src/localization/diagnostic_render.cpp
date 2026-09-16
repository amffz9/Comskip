#include "diagnostic_render.h"
#include <stdexcept>

namespace comskip::localization {
std::string render_diagnostic(const diagnostics::Diagnostic& diagnostic, const Translator& translator) {
    return diagnostics::format_template(translator.text(diagnostics::message_id(diagnostic.code)),
                                        diagnostic.arguments);
}
std::string render_exception(const std::exception& error, const Translator& translator) {
    if (const auto* diagnostic = dynamic_cast<const diagnostics::DiagnosticProvider*>(&error)) {
        try { return render_diagnostic(diagnostic->diagnostic(), translator); }
        catch(const std::exception&) { return diagnostics::english_message(diagnostic->diagnostic()); }
    }
    // Preserve external library/operating-system detail under a localized label.
    return translator.format("diag_external_error", error.what());
}
}
