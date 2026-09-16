#include "output/output_file.h"
#include "output/checked_file.h"
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

TEST(OutputFile, CheckedCFileReportsOwnedWriteFailureAndReleasesHandle)
{
    const auto path=temporary_directory();
    { std::ofstream initial(path,std::ios::binary); initial << "original"; }
    auto file=comskip::platform::own_file(std::fopen(path.string().c_str(),"rb"));
    ASSERT_TRUE(file); ASSERT_EQ(std::setvbuf(file.get(),nullptr,_IONBF,0),0);
    try {
        comskip::output::checked_fprintf(*file,path.string(),"replacement\n");
        FAIL() << "Expected an output-write diagnostic";
    } catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::output_write);
        ASSERT_EQ(error.diagnostic().arguments.size(),1u);
        EXPECT_EQ(error.diagnostic().arguments.front(),path.string());
    }
    file.reset();
    EXPECT_TRUE(std::filesystem::remove(path));
}

TEST(OutputFile, CheckedCloseConsumesOwnershipAndPreservesBytes)
{
    const auto path=temporary_directory();
    auto file=comskip::platform::own_file(std::fopen(path.string().c_str(),"wb"));
    ASSERT_TRUE(file);
    comskip::output::checked_fprintf(*file,path.string(),"complete\n");
    EXPECT_NO_THROW(comskip::output::checked_close(file,path.string()));
    EXPECT_FALSE(file);
    std::ifstream input(path,std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>{input},{}),"complete\n");
    input.close(); EXPECT_TRUE(std::filesystem::remove(path));
}
