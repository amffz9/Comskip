#include "fingerprint/store.h"
#include <sqlite3.h>
#include <cstring>
#include <string>

namespace comskip::fingerprint {
namespace {
// Version 1: recordings with their context and evidence, runs of codes, entries
// and airings. Columns not named by the plan are the implementer's choice.
constexpr const char* schema_v1 = R"sql(
CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE recording(
    id TEXT PRIMARY KEY,
    source TEXT NOT NULL,             -- capture or plex
    channel TEXT,                     -- NULL when unknown
    air_start REAL NOT NULL,          -- Unix seconds of capture second 0
    context TEXT NOT NULL,            -- the Context as JSON
    evidence TEXT,                    -- the latest Evidence as JSON, NULL when none
    file_size INTEGER,
    file_mtime REAL);
CREATE TABLE run(
    recording TEXT NOT NULL REFERENCES recording(id),
    start REAL NOT NULL,              -- capture seconds of code 0
    codes BLOB NOT NULL,              -- 32-bit little-endian codes
    PRIMARY KEY(recording, start));
CREATE TABLE entry(
    id INTEGER PRIMARY KEY,
    status TEXT NOT NULL,
    length REAL NOT NULL,             -- seconds
    codes BLOB NOT NULL,              -- the reference copy
    first_airing REAL,                -- Unix seconds
    last_airing REAL,
    airings INTEGER NOT NULL DEFAULT 0,
    series_keys TEXT NOT NULL DEFAULT '',
    channels TEXT NOT NULL DEFAULT '',
    reason TEXT NOT NULL);
CREATE TABLE airing(
    id INTEGER PRIMARY KEY,
    entry INTEGER NOT NULL REFERENCES entry(id),
    recording TEXT NOT NULL REFERENCES recording(id),
    run_start REAL NOT NULL,
    code_begin INTEGER NOT NULL,      -- codes code_begin..code_end-1 of that run
    code_end INTEGER NOT NULL,
    start REAL NOT NULL,              -- capture seconds
    "end" REAL NOT NULL,
    part_begin INTEGER NOT NULL,      -- the entry's codes it covers
    part_end INTEGER NOT NULL,
    cut_at_start INTEGER NOT NULL,    -- stopped at a run's edge, not a disagreement
    cut_at_end INTEGER NOT NULL,
    found_by TEXT NOT NULL,           -- pass, live or seed
    seam_start INTEGER,               -- NULL without evidence
    seam_end INTEGER,
    logo TEXT NOT NULL,               -- yes, no or unknown
    logo_fraction REAL);
CREATE INDEX airing_entry ON airing(entry);
CREATE INDEX airing_recording ON airing(recording);
INSERT INTO meta VALUES('schema', '1');
)sql";

std::string message(sqlite3* database, std::string_view what) {
    std::string text(what);
    if (database) {
        text += ": ";
        text += sqlite3_errmsg(database);
    }
    return text;
}
}

Store::Store(const std::string& path, int wait_ms) : wait_ms_(wait_ms) {
    const int opened = sqlite3_open_v2(path.c_str(), &database_,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    if (opened != SQLITE_OK) {
        const auto text = message(database_, "Store " + path + " cannot be opened");
        sqlite3_close(database_);
        database_ = nullptr;
        if (opened == SQLITE_NOTADB || opened == SQLITE_CORRUPT) throw StoreCorrupt(text);
        throw StoreUnavailable(text);
    }
    try {
        sqlite3_busy_timeout(database_, wait_ms_);
        sqlite3_extended_result_codes(database_, 0);
        execute("PRAGMA foreign_keys = ON");
        // Reading the journal mode reads the header, so a file that is not a
        // database fails here as StoreCorrupt.
        {
            auto mode = prepare("PRAGMA journal_mode = WAL");
            if (!mode.step() || mode.text(0) != "wal") throw StoreUnavailable("Store " + path + " cannot use write-ahead logging");
        }
        create_or_upgrade();
    } catch (...) {
        sqlite3_close(database_);
        database_ = nullptr;
        throw;
    }
}

Store::~Store() { sqlite3_close(database_); }

void Store::fail(int code, std::string_view what) const {
    const auto text = message(database_, what);
    switch (code & 0xff) {
    case SQLITE_CORRUPT:
    case SQLITE_NOTADB:
    case SQLITE_FORMAT: throw StoreCorrupt(text);
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
    case SQLITE_CANTOPEN:
    case SQLITE_IOERR:
    case SQLITE_READONLY:
    case SQLITE_FULL:
    case SQLITE_PERM: throw StoreUnavailable(text);
    default: throw StoreCorrupt(text);
    }
}

// Most opens find a current store; checking it is a plain read, so opening
// never waits for a writer.
bool Store::current() {
    auto tables = prepare("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = 'meta'");
    tables.step();
    if (tables.integer(0) == 0) return false;
    auto version = prepare("SELECT value FROM meta WHERE key = 'schema'");
    if (!version.step()) return false;
    const auto number = std::stoi(version.text(0));
    if (number > schema_version)
        throw StoreTooNew("Store schema " + std::to_string(number) + " is newer than this tool's " +
                          std::to_string(schema_version));
    return number == schema_version;
}

void Store::create_or_upgrade() {
    if (current()) return;
    in_transaction(*this, [&] {
        auto tables = prepare("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = 'meta'");
        tables.step();
        if (tables.integer(0) == 0) {
            auto any = prepare("SELECT count(*) FROM sqlite_master");
            any.step();
            if (any.integer(0) != 0) throw StoreCorrupt("Store has tables but no schema version");
            execute(schema_v1);
            return;
        }
        auto version = prepare("SELECT value FROM meta WHERE key = 'schema'");
        if (!version.step()) throw StoreCorrupt("Store has no schema version");
        const auto number = std::stoi(version.text(0));
        if (number > schema_version)
            throw StoreTooNew("Store schema " + std::to_string(number) + " is newer than this tool's " +
                              std::to_string(schema_version));
        if (number < 1) throw StoreCorrupt("Store schema version is not valid");
        // Later versions upgrade here, one step at a time.
    });
}

Statement Store::prepare(std::string_view sql) {
    sqlite3_stmt* statement = nullptr;
    const int result = sqlite3_prepare_v2(database_, sql.data(), static_cast<int>(sql.size()), &statement, nullptr);
    if (result != SQLITE_OK) fail(result, "Store statement failed");
    return Statement(*this, statement);
}

void Store::execute(std::string_view sql) {
    const std::string text(sql);
    const int result = sqlite3_exec(database_, text.c_str(), nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) fail(result, "Store statement failed");
}

void Store::begin() {
    if (depth_ == 0) execute("BEGIN IMMEDIATE");
    ++depth_;
}
void Store::commit() {
    if (--depth_ == 0) execute("COMMIT");
}
void Store::rollback() noexcept {
    if (--depth_ == 0) sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr);
}
void Store::checkpoint() noexcept {
    if (depth_ > 0) return;  // the outermost transaction's caller checkpoints
    // A truncating checkpoint waits for readers through the busy handler, so
    // it runs without one: busy means another process is using the log.
    sqlite3_busy_timeout(database_, 0);
    sqlite3_wal_checkpoint_v2(database_, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
    sqlite3_busy_timeout(database_, wait_ms_);
}
long long Store::last_insert_id() const { return sqlite3_last_insert_rowid(database_); }

Statement::~Statement() { sqlite3_finalize(statement_); }
Statement::Statement(Statement&& other) noexcept : store_(other.store_), statement_(other.statement_) {
    other.statement_ = nullptr;
}

Statement& Statement::bind(int index, std::nullptr_t) {
    if (const int result = sqlite3_bind_null(statement_, index); result != SQLITE_OK) store_->fail(result, "Store bind failed");
    return *this;
}
Statement& Statement::bind(int index, long long value) {
    if (const int result = sqlite3_bind_int64(statement_, index, value); result != SQLITE_OK)
        store_->fail(result, "Store bind failed");
    return *this;
}
Statement& Statement::bind(int index, double value) {
    if (const int result = sqlite3_bind_double(statement_, index, value); result != SQLITE_OK)
        store_->fail(result, "Store bind failed");
    return *this;
}
Statement& Statement::bind(int index, std::string_view value) {
    if (const int result = sqlite3_bind_text(statement_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        result != SQLITE_OK)
        store_->fail(result, "Store bind failed");
    return *this;
}
Statement& Statement::bind_codes(int index, std::span<const std::uint32_t> codes) {
    std::vector<unsigned char> bytes(codes.size() * 4);
    for (std::size_t code = 0; code < codes.size(); ++code)
        for (int byte = 0; byte < 4; ++byte) bytes[code * 4 + byte] = static_cast<unsigned char>(codes[code] >> (8 * byte));
    // An empty blob still needs a pointer, or SQLite binds NULL.
    static constexpr unsigned char nothing = 0;
    const void* data = bytes.empty() ? &nothing : bytes.data();
    if (const int result = sqlite3_bind_blob(statement_, index, data, static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
        result != SQLITE_OK)
        store_->fail(result, "Store bind failed");
    return *this;
}

bool Statement::step() {
    const int result = sqlite3_step(statement_);
    if (result == SQLITE_ROW) return true;
    if (result == SQLITE_DONE) return false;
    store_->fail(result, "Store step failed");
}
void Statement::run() {
    while (step()) {}
    reset();
}
void Statement::reset() {
    sqlite3_reset(statement_);
    sqlite3_clear_bindings(statement_);
}

bool Statement::is_null(int column) const { return sqlite3_column_type(statement_, column) == SQLITE_NULL; }
long long Statement::integer(int column) const { return sqlite3_column_int64(statement_, column); }
double Statement::real(int column) const { return sqlite3_column_double(statement_, column); }
std::string Statement::text(int column) const {
    const auto* text = sqlite3_column_text(statement_, column);
    return text ? std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(sqlite3_column_bytes(statement_, column)))
                : std::string();
}
std::vector<std::uint32_t> Statement::codes(int column) const {
    const auto* bytes = static_cast<const unsigned char*>(sqlite3_column_blob(statement_, column));
    const auto size = static_cast<std::size_t>(sqlite3_column_bytes(statement_, column));
    if (size % 4) store_->fail(SQLITE_CORRUPT, "Store codes are not whole 32-bit values");
    std::vector<std::uint32_t> codes(size / 4);
    for (std::size_t code = 0; code < codes.size(); ++code)
        for (int byte = 0; byte < 4; ++byte) codes[code] |= static_cast<std::uint32_t>(bytes[code * 4 + byte]) << (8 * byte);
    return codes;
}
}
