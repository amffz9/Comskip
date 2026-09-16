#include "arguments.h"
#include "analysis.h"
#include "recording_context.h"
#include <memory>
#include "exit_requested.h"
#include <cstdio>
#include <exception>
#include "diagnostic_render.h"

namespace {
int report_error(const std::exception& error, const RecordingContext* context) noexcept {
    try {
        const auto print=[&](const auto& translator) {
            const auto reason=comskip::localization::render_exception(error,translator);
            fputs(translator.format("diag_application_error",reason).c_str(),stderr);
        };
        if(context) print(context->translator);
        else print(comskip::localization::Translator("en"));
    } catch(const std::exception&) {
        // Reporting failures must not terminate the application while unwinding.
        std::fprintf(stderr,"Comskip: %s\n",error.what());
    }
    if(const auto* provider=dynamic_cast<const comskip::diagnostics::DiagnosticProvider*>(&error)) {
        const auto code=provider->diagnostic().code;
        if(code==comskip::diagnostics::Code::output_open || code==comskip::diagnostics::Code::output_write)
            return 6;
    }
    return 2;
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::unique_ptr<RecordingContext> context;
    try {
        comskip::Arguments arguments(argc, argv);
        context = std::make_unique<RecordingContext>();
        context->translator=comskip::localization::Translator::fallback_from_arguments(arguments.size(),arguments.data());
        return comskip_main(*context, arguments.size(), arguments.data());
    } catch (const comskip::ExitRequested& request) {
        return request.status();
    } catch (const std::exception& error) {
        return report_error(error,context.get());
    }
}
#else
int main(int argc, char** argv) {
    std::unique_ptr<RecordingContext> context;
    try {
        context=std::make_unique<RecordingContext>();
        context->translator=comskip::localization::Translator::fallback_from_arguments(argc,argv);
        return comskip_main(*context,argc,argv);
    }
    catch (const comskip::ExitRequested& request) { return request.status(); }
    catch (const std::exception& error) {
        return report_error(error,context.get());
    }
}
#endif
