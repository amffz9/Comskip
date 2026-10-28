// FingerprintTool (docs/fingerprint-plan.md): codes audio fed by the TV app,
// keeps the store, finds repeats and writes marks, after a recording or live.
#include "fingerprint/audio_stream.h"
#include "fingerprint/combiner.h"
#include "fingerprint/json.h"
#include "fingerprint/library.h"
#include "fingerprint/live.h"
#include "fingerprint/seed.h"
#include "fingerprint/store.h"
#include "output/output_file.h"
#include "platform/utf8_paths.h"
#include <algorithm>
#include <cerrno>
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
#else
#include <unistd.h>
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
    "       comskip-fingerprint pass   --store PATH --recording ID [--context PATH] [--evidence PATH]... --out DIR\n"
    "       comskip-fingerprint promote --store PATH\n"
    "  seed and pass read the audio stream on standard input and add its codes to the store;\n"
    "  search and pass find repeats and write one MarksFile per capture with evidence into DIR;\n"
    "  each prints the pass result on standard output. --codes also writes the code stream as\n"
    "  JSON (for measurement). --intro-credits turns on the intro and credits marks (step T4b).\n"
    "  promote applies the promotion rules again to every entry and prints what changed.\n"
    "  --audio-series N and --adjacent N set the audio-only thresholds (for measurement;\n"
    "  defaults 3 and 2, --adjacent 0 turns joining breaks off).\n";

struct Options {
    std::string command;
    std::string store;
    std::string recording;
    std::string context;
    std::string codes;
    std::string out;
    std::string marks;
    std::string log;
    std::vector<std::string> evidence;
    bool intro_credits{};
    PromotionRules rules;
    double stall_at = -1;
    double stall_for{};
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
        else if (name == "--marks") options.marks = value;
        else if (name == "--log") options.log = value;
        else if (name == "--audio-series" || name == "--adjacent") {
            int number = 0;
            try {
                std::size_t used = 0;
                number = std::stoi(value, &used);
                if (used != value.size()) return std::nullopt;
            } catch (const std::exception&) {
                return std::nullopt;
            }
            if (number < 0) return std::nullopt;
            (name == "--adjacent" ? options.rules.adjacent_broadcasts : options.rules.audio_series) = number;
        }
        else if (name == "--stall") {
            const auto colon = value.find(':');
            if (colon == std::string::npos) return std::nullopt;
            try {
                options.stall_at = std::stod(value.substr(0, colon));
                options.stall_for = std::stod(value.substr(colon + 1));
            } catch (const std::exception&) {
                return std::nullopt;
            }
            if (options.stall_at < 0 || options.stall_for < 0) return std::nullopt;
        } else return std::nullopt;
    }
    if (options.command == "promote")
        return options.recording.empty() && options.context.empty() && options.codes.empty() && options.out.empty() &&
                       options.marks.empty() && options.log.empty() && options.evidence.empty() && options.stall_at < 0
                   ? std::optional(options)
                   : std::nullopt;
    if (options.store.empty() || options.recording.empty()) return std::nullopt;
    if (options.command == "live")
        return options.evidence.size() == 1 && !options.marks.empty() && options.out.empty() && options.codes.empty()
                   ? std::optional(options)
                   : std::nullopt;
    if (!options.marks.empty() || !options.log.empty() || options.stall_at >= 0) return std::nullopt;
    const bool audio = options.command == "seed" || options.command == "pass";
    const bool marks = options.command == "search" || options.command == "pass";
    if (!audio && !marks) return std::nullopt;
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

// Live audio is taken as it arrives: a read returns what the pipe holds rather
// than waiting to fill the buffer.
std::size_t read_available(std::span<std::byte> buffer) {
    for (;;) {
#ifdef _WIN32
        const int size = _read(0, buffer.data(), static_cast<unsigned>(std::min<std::size_t>(buffer.size(), 1u << 20)));
#else
        const auto size = ::read(0, buffer.data(), buffer.size());
        if (size < 0 && errno == EINTR) continue;
#endif
        if (size < 0) throw std::runtime_error("Standard input cannot be read");
        return static_cast<std::size_t>(size);
    }
}

int run_live(const Options& options, Context context) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    LiveOptions live_options{options.store, options.recording, std::move(context), options.evidence.front(), options.marks,
                             options.log, options.intro_credits, options.stall_at, options.stall_for};
    const auto result = live(live_options, read_available);
    std::fputs(live_result_json(options.recording, options.marks, result).c_str(), stdout);
    return std::fflush(stdout) == 0 ? 0 : exit_failure;
}

int run_promote(const Options& options) {
    if (options.store.empty()) throw InputInvalid("promote needs --store");
    Store store(options.store);
    Library library(store, {options.intro_credits, "pass", options.rules});
    const auto result = library.promote_all();
    std::map<std::string, int> counts;
    for (const auto& entry : library.entries()) ++counts[std::string(status_name(entry.status))];
    const json::Value document = {{"promoted", result.promoted},
                                  {"changedEntries", result.changed_entries.size()},
                                  {"corrected", result.corrected},
                                  {"entries", counts}};
    std::fputs((json::write(document) + "\n").c_str(), stdout);
    return std::fflush(stdout) == 0 ? 0 : exit_failure;
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
    if (options.command == "live") return run_live(options, context);
    if (options.command == "promote") return run_promote(options);
    std::map<std::string, Evidence> evidence;
    for (const auto& path : options.evidence) {
        auto copy = input([&] { return evidence_from_json(json::parse_file(path)); });
        if (copy.capture_id.empty()) throw InputInvalid(path + ": the evidence copy has no captureId");
        evidence.insert_or_assign(copy.capture_id, std::move(copy));
    }

    Store store(options.store);
    Library library(store, {options.intro_credits, options.command == "pass" ? "pass" : "seed", options.rules});
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
    // Every capture whose Evidence copy was given and whose stream is stored is
    // combined, changed in this run or not, so a rerun for needsEvidence (after
    // this run already changed the entries) still writes the corrected files.
    std::vector<std::string> captures;
    if (own != evidence.end()) captures.push_back(options.recording);
    for (const auto& [capture, copy] : evidence)
        if (capture != options.recording && library.has_stream(capture)) captures.push_back(capture);
    std::vector<std::string> needs_evidence;
    for (const auto& corrected : repeats.corrected)
        if (!evidence.contains(corrected)) needs_evidence.push_back(corrected);
    std::filesystem::create_directories(comskip::platform::path_from_utf8(options.out));
    auto files = json::Value::array();
    for (const auto& capture : captures) {
        const auto& copy = evidence.at(capture);
        std::vector<CombinerMatch> matches;
        for (const auto& match : library.matches(capture)) matches.push_back(agreed_match(match.start, match.end, match.status));
        const auto marks = combine(matches, &copy, {});
        const auto path = comskip::platform::path_to_utf8(comskip::platform::path_from_utf8(options.out) / file_name_for(capture));
        comskip::output::replace_output_file(path, marks_file_json(capture, copy.revision, copy.final, marks));
        files.push_back({{"captureId", capture}, {"path", path}});
    }

    const json::Value document = {{"recording", options.recording},
                                  {"ended", audio.ended},
                                  {"codes", audio.codes()},
                                  {"marksFiles", files},
                                  {"needsEvidence", needs_evidence},
                                  {"promoted", repeats.promoted},
                                  {"airingsFound", repeats.airings_found},
                                  {"droppedAudioSeconds", 0.0}};
    const auto result = json::write(document) + "\n";
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
