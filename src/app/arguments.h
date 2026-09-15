#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace comskip {
// Owns mutable, null-terminated argv storage for the legacy command-line parser.
class Arguments {
    std::vector<std::string> strings_;
    std::vector<char*> pointers_;
public:
#ifdef _WIN32
    Arguments(int count, wchar_t** values) {
        strings_.reserve(count);
        pointers_.reserve(count + 1);
        for (int i = 0; i < count; ++i) {
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
