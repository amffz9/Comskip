#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>
#include "diagnostic.h"

namespace comskip {
inline std::vector<std::string> snapshot_arguments(int count, char* const* values) {
    if (count < 0 || (count > 0 && !values))
        throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::invalid_command_line_argument_array);
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (!values[i]) throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::null_command_line_argument);
        result.emplace_back(values[i]);
    }
    return result;
}
// Owns mutable, null-terminated argv storage for the legacy command-line parser.
class Arguments {
    std::vector<std::string> strings_;
    std::vector<char*> pointers_;
public:
#ifdef _WIN32
    Arguments(int count, wchar_t** values) {
        if (count < 0 || (count > 0 && !values))
            throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::invalid_command_line_argument_array);
        strings_.reserve(static_cast<std::size_t>(count));
        pointers_.reserve(static_cast<std::size_t>(count) + 1);
        for (int i = 0; i < count; ++i) {
            if (!values[i])
                throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::null_command_line_argument);
            const auto utf8 = std::filesystem::path(values[i]).u8string();
            strings_.emplace_back(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        }
        for (auto& value : strings_) pointers_.push_back(value.data());
        pointers_.push_back(nullptr);
    }
#endif
    Arguments(const Arguments&) = delete;
    Arguments& operator=(const Arguments&) = delete;
    int size() const { return static_cast<int>(strings_.size()); }
    char** data() { return pointers_.data(); }
};
}
