// db/history.h
// -----------------------------------------------------------------------
// Persists dispatch runs so the /api/history endpoint and Route History
// UI have something to show.
//
// Two backends, chosen at COMPILE time:
//   - USE_SQLITE defined  -> real SQLite database (db/lifeline.db)
//   - otherwise           -> append-only JSON-lines file (db/history.jsonl)
//
// The fallback means students can build and run the whole project with
// zero external libraries; SQLite is a "nice to have" turned on with
// `make USE_SQLITE=1`. The public interface is identical either way.
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

namespace lifeline {

struct HistoryEntry {
    std::string timestamp;   // ISO-ish local time
    std::string event;       // "dispatch" | "disaster" | ...
    int         count = 0;   // e.g. incidents handled
    double      metric = 0;  // e.g. total response km
    std::string detail;      // free-form summary
};

class History {
public:
    // `path` is the .db (SQLite) or .jsonl (fallback) file.
    explicit History(const std::string& path);
    ~History();

    void add(const HistoryEntry& e);
    std::vector<HistoryEntry> recent(int limit = 20);
    bool usingSqlite() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace lifeline
