#include "output/run_log.h"
#include "localization/diagnostic.h"
#include "platform/utf8_paths.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
TEST(RunLog, AppendsExactFooterToUnicodePath) {
    const auto filename=std::string("comskip-run-caf\xc3\xa9-")+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".log";
    const auto path=std::filesystem::temp_directory_path()/comskip::platform::path_from_utf8(filename);
    struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code error;std::filesystem::remove(path,error);} } cleanup{path};
    { std::ofstream initial(path,std::ios::binary); initial << "prefix\n"; }
    const auto bytes=path.u8string();
    comskip::output::write_run_footer({reinterpret_cast<const char*>(bytes.data()),bytes.size()},"Thu Jan  1 00:00:00 1970\n");
    std::ifstream input(path,std::ios::binary);
    const std::string actual{std::istreambuf_iterator<char>(input),{}};
    EXPECT_EQ(actual,"prefix\n################################################################\nTime at end of run:\nThu Jan  1 00:00:00 1970\n################################################################\n");
}
TEST(RunLog, ReportsBlockedDestinationWithOwnedPath) {
    const auto path=std::filesystem::temp_directory_path()/
        ("comskip-run-blocked-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(path));
    struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);} } cleanup{path};
    try { comskip::output::write_run_footer(path.string(),"time\n"); FAIL() << "Expected open failure"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::output_open);
        ASSERT_EQ(error.diagnostic().arguments.size(),1u);
        EXPECT_EQ(error.diagnostic().arguments.front(),path.string());
    }
}
}
