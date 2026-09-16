#pragma once
#include "localization/diagnostic.h"
#include "platform/file_resources.h"
#include <cstdio>
#include <ios>
#include <string>
#include <string_view>
namespace comskip::output {
inline void checked_fprintf(std::FILE& file, std::string_view path, const char* text) {
    const auto size=std::char_traits<char>::length(text);
    if (std::fwrite(text,1,size,&file) != size)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(diagnostics::Code::output_write,{std::string(path)});
}
template<class... Args> void checked_fprintf(std::FILE& file, std::string_view path, const char* format, Args... args) {
    if (std::fprintf(&file,format,args...) < 0)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(diagnostics::Code::output_write,{std::string(path)});
}
inline void checked_flush(std::FILE& file, std::string_view path) {
    if (std::fflush(&file) != 0)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(diagnostics::Code::output_write,{std::string(path)});
}
inline void checked_close(platform::FilePtr& file, std::string_view path) {
    if (!file) return;
    auto* closing=file.release();
    if (std::fclose(closing) != 0)
        throw diagnostics::DiagnosticError<std::ios_base::failure>(diagnostics::Code::output_write,{std::string(path)});
}
}
