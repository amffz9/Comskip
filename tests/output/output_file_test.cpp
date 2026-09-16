#include "output/output_file.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>

namespace {
std::filesystem::path temporary_directory()
{
    return std::filesystem::temp_directory_path() /
           ("comskip-output-file-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            "-" + std::to_string(std::random_device{}()));
}
}

TEST(OutputFile, WritesExactUtf8BytesAndReplacesExistingContents)
{
    const auto directory = temporary_directory();
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto path = directory / std::filesystem::path{u8"résultat.txt"};
    const auto filename = comskip::platform::path_to_utf8(path);

    comskip::output::write_output_file(filename, "old trailing bytes");
    comskip::output::write_output_file(filename, "new\n");

    std::ifstream input(path, std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>{input}, {}), "new\n");
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}

TEST(OutputFile, MissingParentProducesOwnedOpenDiagnosticAfterOptionalRetry)
{
    const auto directory = temporary_directory();
    const auto filename = comskip::platform::path_to_utf8(directory / "missing" / "result.txt");
    try {
        comskip::output::write_output_file(filename, "data", std::chrono::milliseconds{1});
        FAIL() << "Expected an output-open diagnostic";
    } catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code, comskip::diagnostics::Code::output_open);
        ASSERT_EQ(error.diagnostic().arguments.size(), 1U);
        EXPECT_EQ(error.diagnostic().arguments.front(), filename);
    }
}
