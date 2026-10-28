// FingerprintTool (docs/fingerprint-plan.md): codes audio fed by the TV app.
// Step F3 has `seed` only; the Library, `pass`, `search` and `live` follow.
#include "fingerprint/audio_stream.h"
#include "fingerprint/seed.h"
#include "output/output_file.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#include "arguments.h"
#include <fcntl.h>
#include <io.h>
#endif

namespace {
// Exit codes shared with the TV app ("Cross-repo formats").
constexpr int exit_input_invalid = 2;
constexpr int exit_failure = 6;

constexpr const char* usage =
    "usage: comskip-fingerprint seed --recording ID --codes PATH\n"
    "  Codes the audio stream on standard input and writes its code stream to PATH\n"
    "  (replaced whole); prints the pass result on standard output.\n";

int run(int argc, char** argv) {
    std::string_view command = argc > 1 ? argv[1] : "";
    std::string recording;
    std::string codes_path;
    for (int index = 2; index + 1 < argc; index += 2) {
        const std::string_view name = argv[index];
        if (name == "--recording") recording = argv[index + 1];
        else if (name == "--codes") codes_path = argv[index + 1];
        else command = "";
    }
    if (command != "seed" || argc % 2 || recording.empty() || codes_path.empty()) {
        std::fputs(usage, stderr);
        return exit_failure;
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    const auto result = comskip::fingerprint::seed([](std::span<std::byte> buffer) {
        const auto size = std::fread(buffer.data(), 1, buffer.size(), stdin);
        if (size == 0 && std::ferror(stdin)) throw std::runtime_error("Standard input cannot be read");
        return size;
    });
    comskip::output::replace_output_file(codes_path, comskip::fingerprint::code_stream_json(recording, result));
    const auto summary = comskip::fingerprint::seed_result_json(recording, result);
    std::fputs(summary.c_str(), stdout);
    return std::fflush(stdout) == 0 ? 0 : exit_failure;
}

int report(const std::exception& error, int status) {
    std::fprintf(stderr, "comskip-fingerprint: %s\n", error.what());
    return status;
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        comskip::Arguments arguments(argc, argv);
        return run(arguments.size(), arguments.data());
    } catch (const comskip::fingerprint::InputInvalid& error) {
        return report(error, exit_input_invalid);
    } catch (const std::exception& error) {
        return report(error, exit_failure);
    }
}
#else
int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const comskip::fingerprint::InputInvalid& error) {
        return report(error, exit_input_invalid);
    } catch (const std::exception& error) {
        return report(error, exit_failure);
    }
}
#endif
