// ds/max_heap.h
// -----------------------------------------------------------------------
// Custom binary MAX-heap for the emergency triage queue. Highest priority
// (most severe incident) is served first. Like min_heap.h we deliberately
// avoid std::priority_queue — the whole point of the project is that WE
// implement the heap.
//
// Priority is an integer supplied per item via a key-extractor, so the
// same heap works for incidents (severity), vehicles, etc. Ties broken by
// a monotonic sequence number so equal-severity incidents are served in
// arrival order (stable, FIFO within a priority level).
//
// push O(log n) | pop O(log n) | peek O(1)
// -----------------------------------------------------------------------
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace lifeline {

template <typename T>
class MaxHeap {
public:
    // keyFn(item) -> int priority; larger = served first.
    explicit MaxHeap(std::function<int(const T&)> keyFn)
        : keyFn_(std::move(keyFn)) {}

    bool   empty() const { return data_.empty(); }
    size_t size()  const { return data_.size(); }
    const T& peek() const {
        assert(!data_.empty() && "peek() on empty MaxHeap");
        return data_.front().item;
    }

    void push(const T& item) {
        data_.push_back({item, keyFn_(item), seq_++});
        siftUp(data_.size() - 1);
    }

    T pop() {
        assert(!data_.empty() && "pop() on empty MaxHeap");
        T top = data_.front().item;
        data_.front() = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return top;
    }

private:
    struct Slot {
        T             item;
        int           priority;
        std::uint64_t seq;      // insertion order, for stable ties
    };

    std::vector<Slot>            data_;
    std::function<int(const T&)> keyFn_;
    std::uint64_t                seq_ = 0;

    // a ranks ABOVE b if higher priority, or same priority but inserted
    // earlier (smaller seq).
    bool higher(const Slot& a, const Slot& b) const {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.seq < b.seq;
    }

    void siftUp(size_t i) {
        while (i > 0) {
            const size_t parent = (i - 1) / 2;
            if (higher(data_[i], data_[parent])) {
                std::swap(data_[i], data_[parent]);
                i = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(size_t i) {
        const size_t n = data_.size();
        while (true) {
            const size_t l = 2 * i + 1, r = 2 * i + 2;
            size_t best = i;
            if (l < n && higher(data_[l], data_[best])) best = l;
            if (r < n && higher(data_[r], data_[best])) best = r;
            if (best == i) break;
            std::swap(data_[i], data_[best]);
            i = best;
        }
    }
};

}  // namespace lifeline
