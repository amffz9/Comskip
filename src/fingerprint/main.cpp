// FingerprintTool (docs/fingerprint-plan.md): codes audio fed by the TV app,
// keeps the store, finds repeats and writes marks. `pass` and `live` follow.
#include "fingerprint/audio_stream.h"
#include "fingerprint/combiner.h"
#include "fingerprint/json.h"
#include "fingerprint/library.h"
#include "fingerprint/seed.h"
#include "fingerprint/store.h"
#include "output/output_file.h"
#include "platform/utf8_paths.h"
#include <cstdio>
#include <exception>
#include <filesystem>
#include <map>
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
    "usage: comskip-fingerprint seed   --store PATH --recording ID [--context PATH] [--codes PATH]\n"
    "       comskip-fingerprint search --store PATH --recording ID [--evidence PATH]... --out DIR\n"
    "  seed reads the audio stream on standard input and adds its codes to the store; search\n"
    "  finds repeats and writes one MarksFile per capture with evidence into DIR. Each prints\n"
    "  the pass result on standard output. --codes also writes the code stream as JSON (for\n"
    "  measurement). --intro-credits turns on the intro and credits rule (step T4b).\n";

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
    const bool audio = options.command == "seed";
    const bool marks = options.command == "search";
    if (!audio && !marks) return std::nullopt;
    if (options.store.empty() || options.recording.empty()) return std::nullopt;
    if (marks != !options.out.empty()) return std::nullopt;
    if (!audio && (!options.context.empty() || !options.codes.empty())) return std::nullopt;
    if (!marks && !options.evidence.empty()) return std::nullopt;
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

std::string file_name_for(std::string_view capture) {
    std::string name;
    for (const char character : capture) {
        const bool plain = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                           (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.';
        name += plain ? character : '_';
    }
    return name + ".marks.json";
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
    std::map<std::string, Evidence> evidence;
    for (const auto& path : options.evidence) {
        auto copy = input([&] { return evidence_from_json(json::parse_file(path)); });
        if (copy.capture_id.empty()) throw InputInvalid(path + ": the evidence copy has no captureId");
        evidence.insert_or_assign(copy.capture_id, std::move(copy));
    }

    Store store(options.store);
    Library library(store, {options.intro_credits, "seed"});
    SeedResult audio;
    audio.ended = true;
    if (options.command != "search") {
        audio = read_audio();
        library.add_stream(options.recording, context, audio.runs);
        if (!options.codes.empty())
            comskip::output::replace_output_file(options.codes, code_stream_json(options.recording, audio));
    } else if (!library.has_stream(options.recording)) {
        throw InputInvalid("Recording " + options.recording + " has no stored stream");
    }
    if (options.command == "seed") {
        std::fputs(seed_result_json(options.recording, audio).c_str(), stdout);
        return std::fflush(stdout) == 0 ? 0 : exit_failure;
    }

    const auto own = evidence.find(options.recording);
    const auto repeats = library.find_repeats(options.recording, own == evidence.end() ? nullptr : &own->second);
    std::vector<std::string> captures;
    if (own != evidence.end()) captures.push_back(options.recording);
    std::vector<std::string> needs_evidence;
    for (const auto& corrected : repeats.corrected) {
        if (evidence.contains(corrected)) captures.push_back(corrected);
        else needs_evidence.push_back(corrected);
    }
    std::filesystem::create_directories(comskip::platform::path_from_utf8(options.out));
    std::string files;
    for (const auto& capture : captures) {
        const auto& copy = evidence.at(capture);
        std::vector<CombinerMatch> matches;
        for (const auto& match : library.matches(capture)) matches.push_back(agreed_match(match.start, match.end, match.status));
        const auto marks = combine(matches, &copy, {});
        const auto path = comskip::platform::path_to_utf8(comskip::platform::path_from_utf8(options.out) / file_name_for(capture));
        comskip::output::replace_output_file(path, marks_file_json(capture, copy.revision, copy.final, marks));
        files += files.empty() ? "{ \"captureId\": " : ", { \"captureId\": ";
        json::append_string(files, capture);
        files += ", \"path\": ";
        json::append_string(files, path);
        files += " }";
    }

    std::string result = "{ \"recording\": ";
    json::append_string(result, options.recording);
    result += audio.ended ? ", \"ended\": true, \"codes\": " : ", \"ended\": false, \"codes\": ";
    json::append_integer(result, audio.codes());
    result += ", \"marksFiles\": [" + files + "], \"needsEvidence\": [";
    for (std::size_t index = 0; index < needs_evidence.size(); ++index) {
        if (index) result += ", ";
        json::append_string(result, needs_evidence[index]);
    }
    result += "], \"promoted\": ";
    json::append_integer(result, repeats.promoted);
    result += ", \"airingsFound\": ";
    json::append_integer(result, repeats.airings_found);
    result += ", \"droppedAudioSeconds\": 0.0 }\n";
    std::fputs(result.c_str(), stdout);
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
