#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

// The store file (docs/fingerprint-plan.md, D2): one SQLite database in WAL mode,
// opened only by this tool. This file is the only one that names SQLite.
namespace comskip::fingerprint {
// Exit 3: the store can't be opened or locked within the wait.
struct StoreUnavailable : std::runtime_error {
    using std::runtime_error::runtime_error;
};
// Exit 4: the file is not a store, or is damaged.
struct StoreCorrupt : std::runtime_error {
    using std::runtime_error::runtime_error;
};
// Exit 5: a newer tool made the store.
struct StoreTooNew : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Statement;

class Store {
public:
    static constexpr int schema_version = 1;
    // How long a connection waits for the write lock before StoreUnavailable.
    // Live retries every round, so it waits briefly; a search holds the lock
    // from load to save, a minute or more on the N100, so the batch commands
    // queue behind one another for much longer.
    static constexpr int short_wait_ms = 15000;
    static constexpr int queue_wait_ms = 30 * 60 * 1000;

    // Opens or creates the store, upgrades an older schema and checks the file.
    explicit Store(const std::string& path, int wait_ms = short_wait_ms);
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    Statement prepare(std::string_view sql);
    void execute(std::string_view sql);
    // BEGIN IMMEDIATE, so a writer takes the lock (or waits) before reading.
    // Transactions nest: only the outermost begins, commits or rolls back.
    void begin();
    void commit();
    void rollback() noexcept;
    // Folds the write-ahead log back into the store and empties it; does
    // nothing when a reader is still using the log.
    void checkpoint() noexcept;
    long long last_insert_id() const;
    // Throws the store error that matches an SQLite result code.
    [[noreturn]] void fail(int code, std::string_view what) const;

private:
    void create_or_upgrade();
    bool current();
    sqlite3* database_{};
    int wait_ms_{};
    int depth_{};
};

// Runs a transaction; rolls it back when the body throws.
template <class Body> void in_transaction(Store& store, Body&& body) {
    store.begin();
    try {
        body();
    } catch (...) {
        store.rollback();
        throw;
    }
    store.commit();
}

class Statement {
public:
    Statement(Store& store, sqlite3_stmt* statement) : store_(&store), statement_(statement) {}
    ~Statement();
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&&) = delete;

    Statement& bind(int index, std::nullptr_t);
    Statement& bind(int index, long long value);
    Statement& bind(int index, int value) { return bind(index, static_cast<long long>(value)); }
    Statement& bind(int index, double value);
    Statement& bind(int index, std::string_view value);
    Statement& bind(int index, const char* value) { return bind(index, std::string_view(value)); }
    Statement& bind(int index, const std::string& value) { return bind(index, std::string_view(value)); }
    Statement& bind_codes(int index, std::span<const std::uint32_t> codes);
    // True while a row is available.
    bool step();
    // Runs a statement that returns no rows, then resets it for reuse.
    void run();
    void reset();

    bool is_null(int column) const;
    long long integer(int column) const;
    double real(int column) const;
    std::string text(int column) const;
    std::vector<std::uint32_t> codes(int column) const;

private:
    Store* store_;
    sqlite3_stmt* statement_;
};
}
