#include "fingerprint/live.h"
#include "fingerprint/audio_queue.h"
#include "fingerprint/combiner.h"
#include "fingerprint/json.h"
#include "fingerprint/library.h"
#include "fingerprint/live_matcher.h"
#include "fingerprint/logfmt.h"
#include "fingerprint/process_usage.h"
#include "fingerprint/store.h"
#include "output/output_file.h"
#include "platform/utf8_paths.h"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <tuple>
#include <vector>

namespace comskip::fingerprint {
namespace {
using Clock = std::chrono::steady_clock;
// Flow 14 and 18: the codes are appended and the live index refreshed this often.
constexpr auto store_every = std::chrono::seconds(10);
// The Evidence file is looked at this often (reread within 2 s of a change).
constexpr auto evidence_every = std::chrono::milliseconds(500);
// A usage row is logged this often in stream seconds, and at the end.
constexpr double usage_every = 60;
// The matching thread wakes at least this often while no audio comes.
constexpr auto pop_wait = std::chrono::milliseconds(100);
// A MarksFile a reader holds open (Windows) is retried after this.
constexpr auto replace_retry = std::chrono::milliseconds(20);

// What the matching thread and the store thread share.
struct Shared {
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<CodeRun> runs;  // the recording's codes so far
    bool runs_changed{};
    double now{};  // the capture second reached
    bool stop{};
    std::shared_ptr<const LiveIndex> index;  // a new live index, not yet taken
    std::exception_ptr failure;               // the store can't be used
};

double unix_now() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// Appends the codes and refreshes the live index every 10 s, and once more when
// told to stop. A busy store is retried at the next round.
void keep_store(const LiveOptions& options, Shared& shared) {
    try {
        Store store(options.store);
        Library library(store, {options.intro_credits, "live", {}, options.matching});
        std::optional<std::vector<std::tuple<long long, Status, long long>>> key;
        for (;;) {
            std::vector<CodeRun> runs;
            double now = 0;
            bool stop = false, append = false;
            {
                const std::lock_guard lock(shared.mutex);
                now = shared.now;
                stop = shared.stop;
                append = shared.runs_changed;
                if (append) runs = shared.runs;
                shared.runs_changed = false;
            }
            // The live index ages by air time: the capture's own clock where the
            // app gives it, which is today's date for a live capture.
            const double when = options.context.air_start > 0 ? options.context.air_start + now : unix_now();
            try {
                if (append && !runs.empty()) library.add_stream(options.recording, options.context, runs);
                if (!stop) {
                    auto next = library.live_index_key(when, live_index_code_bytes);
                    if (!key || next != *key) {
                        auto index = std::make_shared<LiveIndex>();
                        library.live_index(when, live_index_code_bytes,
                                           [&](EntryInfo&& entry) { index->add(entry.id, entry.status, entry.codes); });
                        index->finish();
                        const std::lock_guard lock(shared.mutex);
                        shared.index = std::move(index);
                        key = std::move(next);
                    }
                }
            } catch (const StoreUnavailable& error) {
                std::fprintf(stderr, "comskip-fingerprint: %s; retrying in 10 s\n", error.what());
                if (append) {
                    const std::lock_guard lock(shared.mutex);
                    shared.runs_changed = true;
                }
            }
            if (stop) return;
            std::unique_lock lock(shared.mutex);
            shared.wake.wait_for(lock, store_every, [&] { return shared.stop; });
        }
    } catch (...) {
        const std::lock_guard lock(shared.mutex);
        shared.failure = std::current_exception();
    }
}

// The Combiner's matches: index matches as they are; a self-repeat, once it has
// ended, as two ad matches when the within-show rule holds.
std::vector<CombinerMatch> combiner_matches(const std::vector<LiveMatch>& matches, const Evidence& evidence) {
    std::vector<CombinerMatch> found;
    for (const auto& match : matches) {
        if (!match.self()) {
            found.push_back({match.from, match.to, match.agreed_from, match.agreed_to, match.status});
        } else if (match.ended && within_show_ad(evidence, match.earlier_from, match.earlier_to, match.agreed_from, match.agreed_to)) {
            found.push_back(agreed_match(match.earlier_from, match.earlier_to, Status::ad));
            found.push_back(agreed_match(match.agreed_from, match.agreed_to, Status::ad));
        }
    }
    return found;
}

// The --log file: logfmt, one line per event written straight to the file,
// each starting event=, wall= (seconds since the job started) and now= (the
// capture second reached); other times are capture seconds. A marks line (a
// MarksFile written) is followed by one mark line per mark in it.
class Log {
public:
    explicit Log(const std::string& path) {
        if (path.empty()) return;
        file_.open(platform::path_from_utf8(path), std::ios::binary | std::ios::trunc);
        if (!file_) throw std::runtime_error("Log file " + path + " cannot be written");
    }
    bool on() const { return file_.is_open(); }

    void match(std::string_view event, double wall, double now, std::size_t index, const LiveMatch& m) {
        if (!on()) return;
        using namespace logfmt;
        write(line(event, wall, now,
                   {integer("match", static_cast<long long>(index)), integer("entry", m.entry), text("status", status_name(m.status)),
                    seconds("from", m.from), seconds("to", m.to), seconds("agreed_from", m.agreed_from),
                    seconds("agreed_to", m.agreed_to), seconds("earlier_from", m.earlier_from),
                    seconds("earlier_to", m.earlier_to), seconds("stopped_at", m.stopped_at), seconds("ended_at", m.ended_at),
                    flag("ended", m.ended), flag("withdrawn", m.withdrawn)}));
    }
    void revision(std::string_view event, double wall, double now, long long revision) {
        if (!on()) return;
        write(logfmt::line(event, wall, now, {logfmt::integer("revision", revision)}));
    }
    void marks(double wall, double now, long long revision, std::span<const Mark> marks) {
        if (!on()) return;
        using namespace logfmt;
        auto text_lines = line("marks", wall, now, {integer("revision", revision), integer("count", static_cast<long long>(marks.size()))});
        for (const auto& mark : marks)
            text_lines += line("mark", wall, now,
                               {seconds("start", mark.start), seconds("end", mark.end), text("kind", kind_name(mark.kind)),
                                text("state", state_name(mark.state)), text("basis", basis_name(mark.basis))});
        write(text_lines);
    }
    void stall(double wall, double now) {
        if (!on()) return;
        write(logfmt::line("stall", wall, now));
    }
    void end(double wall, double now, long long codes, double dropped, double peak) {
        if (!on()) return;
        using namespace logfmt;
        write(line("end", wall, now, {integer("codes", codes), seconds("dropped_seconds", dropped), seconds("peak_queue_seconds", peak)}));
    }
    // Resource use so far, so a slow leak or a CPU climb shows as a trend.
    void usage(double wall, double now, double dropped, std::size_t index_bytes) {
        if (!on()) return;
        using namespace logfmt;
        const auto usage = process_usage();
        write(line("usage", wall, now,
                   {integer("rss_bytes", static_cast<long long>(usage.rss_bytes)),
                    integer("peak_rss_bytes", static_cast<long long>(usage.peak_rss_bytes)), seconds("cpu_seconds", usage.cpu_seconds),
                    integer("index_bytes", static_cast<long long>(index_bytes)), seconds("dropped_seconds", dropped)}));
    }

private:
    void write(const std::string& text) {
        file_ << text;
        file_.flush();
    }
    std::ofstream file_;
};

// The Evidence file as last seen, so it is reread only when it changes.
struct EvidenceStamp {
    std::filesystem::file_time_type time{};
    std::uintmax_t size{};
    bool operator==(const EvidenceStamp&) const = default;
};
}

LiveResult live(const LiveOptions& options, const ReadBytes& read) {
    const auto started = Clock::now();
    const auto wall = [&] { return std::chrono::duration<double>(Clock::now() - started).count(); };
    Log log(options.log);
    // Reading never waits for matching: the queue drops what it can't hold. The
    // reader owns its share of the queue, since a failed job leaves it running.
    const auto shared_queue = std::make_shared<AudioQueue>(static_cast<std::size_t>(live_queue_seconds * sample_rate));
    auto& queue = *shared_queue;
    std::thread reader([shared_queue, read] {
        try {
            AudioStreamParser parser(*shared_queue);
            std::vector<std::byte> buffer(1 << 14);
            while (!parser.ended()) {
                const auto size = read(buffer);
                if (size == 0) break;
                parser.push(std::span(buffer).first(size));
            }
            shared_queue->close();
        } catch (...) {
            shared_queue->close(std::current_exception());
        }
    });
    Shared shared;
    std::thread store_thread([&] { keep_store(options, shared); });
    // Every way out stops the store thread; the reader is left to the end of
    // the process when the job fails before its input ends.
    struct Threads {
        Shared& shared;
        std::thread& store;
        std::thread& reader;
        AudioQueue& queue;
        ~Threads() {
            {
                const std::lock_guard lock(shared.mutex);
                shared.stop = true;
            }
            shared.wake.notify_all();
            if (store.joinable()) store.join();
            if (reader.joinable()) {
                if (queue.drained()) reader.join();
                else reader.detach();
            }
        }
    } threads{shared, store_thread, reader, queue};

    const auto evidence_path = platform::path_from_utf8(options.evidence);
    LiveResult result;
    LiveMatcher matcher(options.matching);
    MarkHistory history;
    std::optional<Coder> coder;
    std::optional<Evidence> evidence;
    std::optional<EvidenceStamp> stamp;
    auto evidence_checked = Clock::now() - evidence_every;
    std::string written;
    std::vector<std::pair<bool, bool>> logged;  // per match: logged as found, logged as ended
    double run_start = 0, now = 0;
    long long run_samples = 0;
    bool stalled = false;
    double next_usage = usage_every;

    const auto look_at_evidence = [&] {
        evidence_checked = Clock::now();
        std::error_code time_error, size_error;
        const EvidenceStamp seen{std::filesystem::last_write_time(evidence_path, time_error),
                                 std::filesystem::file_size(evidence_path, size_error)};
        if (time_error || size_error || (stamp && *stamp == seen)) return;
        try {
            auto next = evidence_from_json(json::parse_file(options.evidence));
            stamp = seen;
            if (evidence && next.revision <= evidence->revision) return;
            evidence = std::move(next);
            log.revision("evidence", wall(), now, evidence->revision);
        } catch (const json::Invalid& invalid) {
            // Being replaced, or broken: the copy in hand stands until a good one comes.
            std::fprintf(stderr, "comskip-fingerprint: live evidence not used: %s\n", invalid.what());
            stamp = seen;
        }
    };
    const auto publish = [&] {
        const auto& matches = matcher.matches();
        if (log.on()) {
            logged.resize(matches.size());
            for (std::size_t index = 0; index < matches.size(); ++index) {
                auto& [found, ended] = logged[index];
                if (!found) log.match("match", wall(), now, index, matches[index]);
                if (!ended && matches[index].ended) log.match("ended", wall(), now, index, matches[index]);
                found = true;
                ended = matches[index].ended;
            }
        }
        if (!evidence) return;
        const auto marks = history.settle(combine(combiner_matches(matches, *evidence), &*evidence, {false, now}), now);
        auto text = marks_file_json(options.recording, evidence->revision, false, marks);
        if (text == written) return;
        try {
            output::replace_output_file(options.marks, text, replace_retry);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "comskip-fingerprint: %s; retrying\n", error.what());
            return;
        }
        written = std::move(text);
        result.marks_written = true;
        log.marks(wall(), now, evidence->revision, marks);
    };

    for (;;) {
        {
            const std::lock_guard lock(shared.mutex);
            if (shared.failure) std::rethrow_exception(shared.failure);
            if (shared.index) matcher.refresh(std::move(shared.index));
            shared.index.reset();
        }
        auto piece = queue.pop(pop_wait);
        if (!piece && queue.drained()) break;
        if (piece) {
            switch (piece->kind) {
            case AudioPiece::Kind::run: {
                coder.emplace();
                matcher.run_start(piece->start);
                run_start = now = piece->start;
                run_samples = 0;
                const std::lock_guard lock(shared.mutex);
                shared.runs.push_back({piece->start, {}});
                break;
            }
            case AudioPiece::Kind::audio: {
                const auto codes = coder->feed(piece->samples);
                run_samples += static_cast<long long>(piece->samples.size());
                now = run_start + static_cast<double>(run_samples) / sample_rate;
                matcher.feed(codes);
                const std::lock_guard lock(shared.mutex);
                auto& run = shared.runs.back().codes;
                run.insert(run.end(), codes.begin(), codes.end());
                shared.runs_changed = shared.runs_changed || !codes.empty();
                shared.now = now;
                break;
            }
            case AudioPiece::Kind::end:
                result.ended = true;
                matcher.finish();
                break;
            }
        }
        if (options.stall_at >= 0 && !stalled && now >= options.stall_at) {
            stalled = true;
            log.stall(wall(), now);
            std::this_thread::sleep_for(std::chrono::duration<double>(options.stall_for));
        }
        if (Clock::now() - evidence_checked >= evidence_every) look_at_evidence();
        publish();
        if (now >= next_usage) {
            log.usage(wall(), now, static_cast<double>(queue.dropped_samples()) / sample_rate, matcher.index_bytes());
            next_usage = now + usage_every;
        }
    }
    if (const auto error = queue.error()) std::rethrow_exception(error);
    matcher.finish();
    look_at_evidence();
    publish();

    {
        const std::lock_guard lock(shared.mutex);
        shared.stop = true;
        shared.runs_changed = true;
    }
    shared.wake.notify_all();
    store_thread.join();
    if (shared.failure) std::rethrow_exception(shared.failure);

    result.codes = matcher.codes();
    for (const auto& match : matcher.matches()) {
        ++result.matches;
        if (match.withdrawn) ++result.withdrawn;
    }
    result.dropped_seconds = static_cast<double>(queue.dropped_samples()) / sample_rate;
    log.usage(wall(), now, result.dropped_seconds, matcher.index_bytes());
    log.end(wall(), now, result.codes, result.dropped_seconds, static_cast<double>(queue.peak_samples()) / sample_rate);
    return result;
}

std::string live_result_json(std::string_view recording, std::string_view marks_path, const LiveResult& result) {
    json::Value document = {{"recording", recording},
                            {"ended", result.ended},
                            {"codes", result.codes},
                            {"marksFiles", json::Value::array()},
                            {"matches", result.matches},
                            {"withdrawn", result.withdrawn},
                            {"droppedAudioSeconds", json::rounded(result.dropped_seconds, 3)}};
    if (result.marks_written) document["marksFiles"].push_back({{"captureId", recording}, {"path", marks_path}});
    return document.dump() + "\n";
}
}
