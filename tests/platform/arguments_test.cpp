#include "arguments.h"
#include <gtest/gtest.h>
#ifdef _WIN32
TEST(Arguments, RejectsInvalidWideArraysAndOwnsEmptySentinel) {
    wchar_t* values[] = {nullptr};
    EXPECT_THROW(comskip::Arguments(-1, values), std::invalid_argument);
    EXPECT_THROW(comskip::Arguments(1, nullptr), std::invalid_argument);
    EXPECT_THROW(comskip::Arguments(1, values), std::invalid_argument);
    comskip::Arguments empty(0, nullptr);
    EXPECT_EQ(empty.size(), 0);
    EXPECT_EQ(empty.data()[0], nullptr);
}
TEST(Arguments, PreservesUnicodeLongArgumentsAndMoreThanTwentyValues) {
    std::vector<std::wstring> values(40, std::wstring(2000, L'x'));
    values[1] = L"C:\\recordings\\\u7535\u89c6 sample.ts";
    std::vector<wchar_t*> pointers;
    for (auto& value : values) pointers.push_back(value.data());
    comskip::Arguments arguments(static_cast<int>(pointers.size()), pointers.data());
    ASSERT_EQ(arguments.size(), 40);
    EXPECT_EQ(std::string(arguments.data()[0]).size(), 2000);
    EXPECT_EQ(std::string(arguments.data()[1]), "C:\\recordings\\\xe7\x94\xb5\xe8\xa7\x86 sample.ts");
    EXPECT_EQ(std::string(arguments.data()[39]), std::string(2000, 'x'));
    EXPECT_EQ(arguments.data()[40], nullptr);
}
#endif
