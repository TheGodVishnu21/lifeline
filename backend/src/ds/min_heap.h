// ds/min_heap.h
// -----------------------------------------------------------------------
// Custom binary min-heap (array-based). We deliberately do NOT use
// std::priority_queue: implementing the heap ourselves is the point of a
// DSA project, and it is a guaranteed viva question.
//
// Complexity:  push O(log n) | pop O(log n) | top O(1)
// Layout:      parent(i) = (i-1)/2, children = 2i+1, 2i+2
//
// Dijkstra/A* use the classic "lazy deletion" trick instead of
// decrease-key: outdated copies of a node are simply skipped when popped.
// -----------------------------------------------------------------------
#pragma once

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace lifeline {

template <typename T>
class MinHeap {
public:
    bool   empty() const { return data_.empty(); }
    size_t size()  const { return data_.size(); }
    const T& top() const {
        assert(!data_.empty() && "top() on empty MinHeap");
        return data_.front();
    }

    void push(const T& value) {
        data_.push_back(value);
        siftUp(data_.size() - 1);
    }

    // Removes and returns the smallest element.
    T pop() {
        assert(!data_.empty() && "pop() on empty MinHeap");
        T root = data_.front();
        data_.front() = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return root;
    }

private:
    std::vector<T> data_;

    void siftUp(size_t i) {
        while (i > 0) {
            const size_t parent = (i - 1) / 2;
            if (data_[i] < data_[parent]) {
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
            size_t smallest = i;
            if (l < n && data_[l] < data_[smallest]) smallest = l;
            if (r < n && data_[r] < data_[smallest]) smallest = r;
            if (smallest == i) break;
            std::swap(data_[i], data_[smallest]);
            i = smallest;
        }
    }
};

}  // namespace lifeline
