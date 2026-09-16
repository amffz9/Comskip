#pragma once
#include "diagnostic.h"
#include "translator.h"

namespace comskip::localization {
std::string render_diagnostic(const diagnostics::Diagnostic&, const Translator&);
std::string render_exception(const std::exception&, const Translator&);
}
