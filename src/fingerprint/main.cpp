// FingerprintTool (docs/fingerprint-plan.md): codes audio fed by the TV app,
// keeps the store. `search`, `pass` and `live` follow.
#include "fingerprint/audio_stream.h"
#include "fingerprint/json.h"
#include "fingerprint/library.h"
#include "fingerprint/seed.h"
#include "fingerprint/store.h"
#include "output/output_file.h"
#include <cstdio>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include "arguments.h"
#include <fcntl.h>
#include <io.h>
#endif

namespace {
using namespace comskip::fingerprint;

// Exit codes shared with the TV app ("Cross-repo formats").
constexpr int exit_input_invalid = 2;
constexpr int exit_store_unavailable = 3;
constexpr int exit_store_corrupt = 4;
constexpr int exit_store_too_new = 5;
constexpr int exit_failure = 6;

constexpr const char* usage =
    "usage: comskip-fingerprint seed --store PATH --recording ID [--context PATH] [--codes PATH]\n"
    "  Reads the audio stream on standard input and adds its codes to the store; prints the\n"
    "  pass result on standard output. --codes also writes the code stream as JSON (for\n"
    "  measurement).\n";

struct Options {
    std::string command;
    std::string store;
    std::string recording;
    std::string context;
    std::string codes;
    std::string out;
    std::vector<std::string> evidence;
    bool intro_credits{};
};

std::optional<Options> parse(int argc, char** argv) {
    Options options;
    if (argc < 2) return std::nullopt;
    options.command = argv[1];
    for (int index = 2; index < argc; ++index) {
        const std::string_view name = argv[index];
        if (name == "--intro-credits") {
            options.intro_credits = true;
            continue;
        }
        if (index + 1 >= argc) return std::nullopt;
        const std::string value = argv[++index];
        if (name == "--store") options.store = value;
        else if (name == "--recording") options.recording = value;
        else if (name == "--context") options.context = value;
        else if (name == "--codes") options.codes = value;
        else if (name == "--out") options.out = value;
        else if (name == "--evidence") options.evidence.push_back(value);
        else return std::nullopt;
    }
    if (options.command != "seed" || options.store.empty() || options.recording.empty()) return std::nullopt;
    if (!options.out.empty() || !options.evidence.empty() || options.intro_credits) return std::nullopt;
    return options;
}

// Inputs the app hands over that break their format are InputInvalid.
template <class Body> auto input(Body&& body) {
    try {
        return body();
    } catch (const json::Invalid& error) {
        throw InputInvalid(error.what());
    }
}

SeedResult read_audio() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    return seed([](std::span<std::byte> buffer) {
        const auto size = std::fread(buffer.data(), 1, buffer.size(), stdin);
        if (size == 0 && std::ferror(stdin)) throw std::runtime_error("Standard input cannot be read");
        return size;
    });
}

int run(int argc, char** argv) {
    const auto parsed = parse(argc, argv);
    if (!parsed) {
        std::fputs(usage, stderr);
        return exit_failure;
    }
    const auto& options = *parsed;
    const auto context = options.context.empty() ? Context{}
                                                 : input([&] { return context_from_json(json::parse_file(options.context)); });
    Store store(options.store);
    Library library(store);
    const auto audio = read_audio();
    library.add_stream(options.recording, context, audio.runs);
    if (!options.codes.empty())
        comskip::output::replace_output_file(options.codes, code_stream_json(options.recording, audio));
    std::fputs(seed_result_json(options.recording, audio).c_str(), stdout);
    return std::fflush(stdout) == 0 ? 0 : exit_failure;
}

int report(const std::exception& error, int status) {
    std::fprintf(stderr, "comskip-fingerprint: %s\n", error.what());
    return status;
}

int guarded(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const InputInvalid& error) {
        return report(error, exit_input_invalid);
    } catch (const StoreUnavailable& error) {
        return report(error, exit_store_unavailable);
    } catch (const StoreCorrupt& error) {
        return report(error, exit_store_corrupt);
    } catch (const StoreTooNew& error) {
        return report(error, exit_store_too_new);
    } catch (const std::exception& error) {
        return report(error, exit_failure);
    }
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        comskip::Arguments arguments(argc, argv);
        return guarded(arguments.size(), arguments.data());
    } catch (const std::exception& error) {
        return report(error, exit_failure);
    }
}
#else
int main(int argc, char** argv) { return guarded(argc, argv); }
#endif
