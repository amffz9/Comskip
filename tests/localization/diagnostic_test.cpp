#include "diagnostic_render.h"
#include <gtest/gtest.h>
#include <ios>
#include <stdexcept>
#include <format>
#include <array>
#include <fstream>
#include <iterator>
#include "../config/legacy_quoted_value.h"
using comskip::diagnostics::DiagnosticError;
using comskip::diagnostics::Code;
TEST(Diagnostic, PreservesExceptionCategoriesAndOwnsArguments) {
    std::string key="max_brightness";
    const DiagnosticError<std::invalid_argument> error(Code::setting_byte,{key});
    key="changed";
    const std::exception& base=error;
    EXPECT_NE(dynamic_cast<const std::invalid_argument*>(&base),nullptr);
    EXPECT_EQ(comskip::localization::render_exception(base,comskip::localization::Translator("es")),
              "max_brightness debe estar entre 0 y 255");
    EXPECT_THROW(throw DiagnosticError<std::out_of_range>(Code::integer_range,{"frame"}),std::out_of_range);
    EXPECT_THROW(throw DiagnosticError<std::length_error>(Code::text_input_line_exceeds_its_limit),std::length_error);
    EXPECT_THROW(throw DiagnosticError<std::ios_base::failure>(Code::failed_writing_edl_output),std::ios_base::failure);
}
TEST(Diagnostic, TranslatesKnownErrorsAndLabelsExternalDetails) {
    const DiagnosticError<std::invalid_argument> error(Code::csv_input_has_no_header);
    EXPECT_EQ(comskip::localization::render_exception(error,comskip::localization::Translator("es")),
              "La entrada CSV no tiene cabecera");
    EXPECT_EQ(comskip::localization::render_exception(error,comskip::localization::Translator("en")),
              "CSV input has no header");
    EXPECT_EQ(comskip::localization::render_exception(std::runtime_error("vendor detail"),
              comskip::localization::Translator("es")),"Error externo: vendor detail");
}
TEST(Diagnostic, SelectedCatalogCanFallbackToEnglishWithoutChangingParameters) {
    comskip::localization::Translator translator("es",comskip::config::Ini("diag_setting_byte=\"{} must be between 0 and 255\""),comskip::config::Ini{});
    EXPECT_EQ(comskip::localization::render_diagnostic({Code::setting_byte,{"brightness"}},translator),
              "brightness must be between 0 and 255");
}
TEST(Diagnostic, EnglishCachePreservesQuotedValueEscapes) {
    const std::string argument="line\n\t\"quote\"\\tail";
    const comskip::diagnostics::Diagnostic diagnostic{Code::application_error,{argument}};
    EXPECT_EQ(comskip::diagnostics::english_message(diagnostic),
              comskip::localization::Translator("en").format("diag_application_error",argument));
    const auto value=comskip::config::detail::decode_legacy_ini_value("\"a\\n\\t\\\"b\\\\c\"");
    ASSERT_TRUE(value);
    EXPECT_EQ(*value,"a\n\t\"b\\c");
}
TEST(Diagnostic, FormattingUsesExactArityAndRejectsMissingArguments) {
    const std::array<std::string,4> values{"a","b","c","d"};
    EXPECT_EQ(comskip::diagnostics::format_template("{} {} {} {}",values),"a b c d");
    EXPECT_THROW(comskip::diagnostics::format_template("{} {}",std::span(values).first(1)),std::format_error);
    const std::array<std::string,9> excessive{};
    EXPECT_THROW(comskip::diagnostics::format_template("{}",excessive),std::length_error);
}
TEST(Diagnostic, EveryCodeHasEnglishAndSpanishCatalogEntries) {
    const auto directory=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"config/locales";
    for(const auto language:{"en","es"}) {
        std::ifstream input(directory/(std::string(language)+".ini"));
        ASSERT_TRUE(input);
        const comskip::config::Ini catalog(std::string{std::istreambuf_iterator<char>(input),{}});
        for(int code=0;code<static_cast<int>(Code::count);++code) {
            const auto id=comskip::diagnostics::message_id(static_cast<Code>(code));
            EXPECT_NE(catalog.find(id),nullptr)<<language<<": "<<id;
        }
    }
}
TEST(Diagnostic, MissingRenderCatalogEntryFallsBackToReadableEnglish) {
    const DiagnosticError<std::invalid_argument> error(Code::csv_input_has_no_header);
    const comskip::localization::Translator translator("es",comskip::config::Ini{},comskip::config::Ini{});
    EXPECT_EQ(comskip::localization::render_exception(error,translator),"CSV input has no header");
    EXPECT_EQ(std::string(error.what()),"CSV input has no header");
}
