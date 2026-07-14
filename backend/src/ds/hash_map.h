// ds/hash_map.h
// -----------------------------------------------------------------------
// Custom hash map (string -> V) using separate chaining. Included so the
// project demonstrates a hand-built hash table, not just std::unordered_map.
// Used by the dispatch layer for O(1) average vehicle-type counts and by
// the resource planner. Auto-resizes (doubles) when load factor > 0.75.
//
// Hash: djb2 (classic, well-distributed for short strings).
// get/put/contains: O(1) average, O(n) worst case (all keys collide).
// -----------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace lifeline {

template <typename V>
class HashMap {
public:
    HashMap() : buckets_(kInitialBuckets) {}

    void put(const std::string& key, const V& value) {
        auto& chain = buckets_[indexFor(key)];
        for (auto& kv : chain)
            if (kv.first == key) { kv.second = value; return; }
        chain.emplace_back(key, value);
        ++count_;
        if (loadFactor() > kMaxLoad) rehash(buckets_.size() * 2);
    }

    bool contains(const std::string& key) const {
        const auto& chain = buckets_[indexFor(key)];
        for (const auto& kv : chain)
            if (kv.first == key) return true;
        return false;
    }

    // Returns pointer to value, or nullptr if absent (no exceptions).
    V* get(const std::string& key) {
        auto& chain = buckets_[indexFor(key)];
        for (auto& kv : chain)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }

    // Value if present, else `fallback` (handy for counters).
    V getOr(const std::string& key, const V& fallback) const {
        const auto& chain = buckets_[indexFor(key)];
        for (const auto& kv : chain)
            if (kv.first == key) return kv.second;
        return fallback;
    }

    size_t size() const { return count_; }
    bool   empty() const { return count_ == 0; }

    // Visit every (key, value) pair — order is bucket order, not
    // insertion order (that's inherent to hash tables).
    template <typename Fn>
    void forEach(Fn fn) const {
        for (const auto& chain : buckets_)
            for (const auto& kv : chain) fn(kv.first, kv.second);
    }

private:
    static constexpr size_t kInitialBuckets = 16;
    static constexpr double kMaxLoad = 0.75;

    std::vector<std::list<std::pair<std::string, V>>> buckets_;
    size_t count_ = 0;

    // djb2 string hash.
    static size_t hash(const std::string& s) {
        size_t h = 5381;
        for (unsigned char c : s) h = ((h << 5) + h) + c;  // h*33 + c
        return h;
    }
    size_t indexFor(const std::string& key) const {
        return hash(key) % buckets_.size();
    }
    double loadFactor() const {
        return static_cast<double>(count_) / buckets_.size();
    }
    void rehash(size_t newSize) {
        std::vector<std::list<std::pair<std::string, V>>> bigger(newSize);
        for (auto& chain : buckets_)
            for (auto& kv : chain)
                bigger[hash(kv.first) % newSize].push_back(std::move(kv));
        buckets_ = std::move(bigger);
    }
};

}  // namespace lifeline
