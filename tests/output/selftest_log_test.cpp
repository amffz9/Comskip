#include "output/selftest_log.h"
#include "settings_value.h"
#include "diagnostic_render.h"
#include "platform/utf8_paths.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
class SelftestLog : public ::testing::Test {
protected:
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("comskip-selftest-log-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    void SetUp() override { ASSERT_TRUE(std::filesystem::create_directory(directory)); }
    void TearDown() override { std::error_code error; std::filesystem::remove_all(directory, error); }
};
}
TEST_F(SelftestLog, AppendsCompleteUnicodeRecordsAndReleasesEachHandle) {
    const auto path = directory / std::filesystem::path(u8"prueba café 日本語.log");
    const auto filename = comskip::platform::path_to_utf8(path);
    comskip::output::write_selftest_log(filename, "seek {:.3f}: {}\n", 1.25, "first");
    comskip::output::write_selftest_log(filename, "{}\n", std::string(5000, 'x'));
    {
        std::ifstream input(path, std::ios::binary);
        const std::string contents{std::istreambuf_iterator<char>(input), {}};
        EXPECT_EQ(contents, "seek 1.250: first\n" + std::string(5000, 'x') + "\n");
    }
    EXPECT_TRUE(std::filesystem::remove(path));
}
TEST_F(SelftestLog, FailedOpenPreservesTypedErrorAndOwnsFilename) {
    const auto filename = comskip::platform::path_to_utf8(directory / "missing" / "seektest.log");
    try { comskip::output::append_selftest_log(filename, "record\n"); FAIL() << "Accepted absent parent"; }
    catch (const std::ios_base::failure& error) {
        const auto* provider = dynamic_cast<const comskip::diagnostics::DiagnosticProvider*>(&error);
        ASSERT_NE(provider, nullptr);
        EXPECT_EQ(provider->diagnostic().code, comskip::diagnostics::Code::output_open);
        EXPECT_EQ(provider->diagnostic().arguments, std::vector<std::string>{filename});
        const auto spanish = comskip::localization::render_exception(error, comskip::localization::Translator("es"));
        EXPECT_NE(spanish.find(filename), std::string::npos);
        EXPECT_NE(spanish.find("No se"), std::string::npos);
    }
    EXPECT_FALSE(std::filesystem::exists(directory / "missing"));
}
TEST_F(SelftestLog, CommittedSettingHasIndependentConfigurableDestination) {
    const auto defaults = comskip::config::default_settings();
    EXPECT_EQ(defaults.selftest_log_file, "seektest.log");
    const auto configured = comskip::config::load_settings(comskip::config::Ini("selftest_log_file=custom.log"), defaults);
    EXPECT_EQ(configured.selftest_log_file, "custom.log");
    EXPECT_EQ(defaults.selftest_log_file, "seektest.log");
}
