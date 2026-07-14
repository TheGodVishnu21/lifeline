#include "db/history.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

#include "json.hpp"

namespace lifeline {

using nlohmann::json;

namespace {
std::string nowStamp() {
    const auto t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}
}  // namespace

// =======================================================================
#ifdef USE_SQLITE
// ------------------------- SQLite backend ------------------------------
#include <sqlite3.h>

struct History::Impl {
    sqlite3* db = nullptr;

    explicit Impl(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            std::fprintf(stderr, "sqlite open failed: %s\n",
                         sqlite3_errmsg(db));
            db = nullptr;
            return;
        }
        const char* ddl =
            "CREATE TABLE IF NOT EXISTS history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp TEXT, event TEXT, count INTEGER,"
            "metric REAL, detail TEXT);";
        char* err = nullptr;
        if (sqlite3_exec(db, ddl, nullptr, nullptr, &err) != SQLITE_OK) {
            std::fprintf(stderr, "sqlite ddl failed: %s\n", err);
            sqlite3_free(err);
        }
    }
    ~Impl() { if (db) sqlite3_close(db); }
};

History::History(const std::string& path) : impl_(new Impl(path)) {}
History::~History() { delete impl_; }
bool History::usingSqlite() const { return impl_->db != nullptr; }

void History::add(const HistoryEntry& e) {
    if (!impl_->db) return;
    const char* sql =
        "INSERT INTO history(timestamp,event,count,metric,detail)"
        " VALUES(?,?,?,?,?);";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK)
        return;
    const std::string ts = e.timestamp.empty() ? nowStamp() : e.timestamp;
    sqlite3_bind_text(st, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, e.event.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, e.count);
    sqlite3_bind_double(st, 4, e.metric);
    sqlite3_bind_text(st, 5, e.detail.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

std::vector<HistoryEntry> History::recent(int limit) {
    std::vector<HistoryEntry> out;
    if (!impl_->db) return out;
    const char* sql =
        "SELECT timestamp,event,count,metric,detail FROM history"
        " ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &st, nullptr) != SQLITE_OK)
        return out;
    sqlite3_bind_int(st, 1, limit);
    while (sqlite3_step(st) == SQLITE_ROW) {
        HistoryEntry e;
        auto txt = [&](int c) {
            const unsigned char* p = sqlite3_column_text(st, c);
            return p ? std::string(reinterpret_cast<const char*>(p)) : "";
        };
        e.timestamp = txt(0);
        e.event = txt(1);
        e.count = sqlite3_column_int(st, 2);
        e.metric = sqlite3_column_double(st, 3);
        e.detail = txt(4);
        out.push_back(std::move(e));
    }
    sqlite3_finalize(st);
    return out;
}

#else
// ------------------- JSON-lines fallback backend -----------------------
namespace {
json toJson(const HistoryEntry& e) {
    return json{{"timestamp", e.timestamp}, {"event", e.event},
                {"count", e.count}, {"metric", e.metric},
                {"detail", e.detail}};
}
HistoryEntry fromJson(const json& j) {
    HistoryEntry e;
    e.timestamp = j.value("timestamp", "");
    e.event = j.value("event", "");
    e.count = j.value("count", 0);
    e.metric = j.value("metric", 0.0);
    e.detail = j.value("detail", "");
    return e;
}
}  // namespace

struct History::Impl {
    std::string path;
    explicit Impl(std::string p) : path(std::move(p)) {}
};

History::History(const std::string& path) : impl_(new Impl(path + ".jsonl")) {}
History::~History() { delete impl_; }
bool History::usingSqlite() const { return false; }

void History::add(const HistoryEntry& e) {
    std::ofstream out(impl_->path, std::ios::app);
    if (!out) return;
    HistoryEntry copy = e;
    if (copy.timestamp.empty()) copy.timestamp = nowStamp();
    out << toJson(copy).dump() << "\n";
}

std::vector<HistoryEntry> History::recent(int limit) {
    std::vector<HistoryEntry> all;
    std::ifstream in(impl_->path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            all.push_back(fromJson(json::parse(line)));
        } catch (...) { /* skip corrupt line */ }
    }
    std::vector<HistoryEntry> out;
    for (auto it = all.rbegin();
         it != all.rend() && static_cast<int>(out.size()) < limit; ++it)
        out.push_back(*it);
    return out;
}
#endif

}  // namespace lifeline
