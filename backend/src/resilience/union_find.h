// resilience/union_find.h
// -----------------------------------------------------------------------
// Disjoint Set Union (Union-Find) with PATH HALVING + UNION BY RANK.
//
// find() amortised ~O(α(n)) — inverse Ackermann, effectively constant.
// Path halving (every node on the walk points to its grandparent) gives
// the same asymptotics as full path compression but stays iterative and
// one line long — easy to defend at a whiteboard.
//
// Used by: Kruskal's MST (cycle detection) and the resilience report
// (how many components is the city in after a disaster?).
// -----------------------------------------------------------------------
#pragma once

#include <utility>
#include <vector>

namespace lifeline {

class UnionFind {
public:
    explicit UnionFind(int n) : parent_(n), rank_(n, 0), components_(n) {
        for (int i = 0; i < n; ++i) parent_[i] = i;
    }

    int find(int x) const {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];   // path halving
            x = parent_[x];
        }
        return x;
    }

    // Returns false if a and b were already in the same set.
    bool unite(int a, int b) {
        a = find(a);
        b = find(b);
        if (a == b) return false;
        if (rank_[a] < rank_[b]) std::swap(a, b);  // attach shorter tree
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
        --components_;
        return true;
    }

    bool connected(int a, int b) const { return find(a) == find(b); }
    int  components() const { return components_; }

private:
    mutable std::vector<int> parent_;   // mutable: find() compresses paths
    std::vector<int>         rank_;
    int                      components_;
};

}  // namespace lifeline
