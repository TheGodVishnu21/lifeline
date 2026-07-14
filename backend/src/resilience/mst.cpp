#include "resilience/mst.h"

#include <chrono>
#include <tuple>
#include <vector>

#include "ds/min_heap.h"
#include "resilience/union_find.h"

namespace lifeline {

namespace {
std::string roadBetween(const Graph& g, int u, int v) {
    for (const Edge& e : g.neighbors(u))
        if (e.to == v) return e.road;
    return "";
}
}  // namespace

MstResult primMst(const Graph& g) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();

    MstResult res;
    res.algo = "prim";
    const int n = g.size();
    std::vector<bool> inTree(n, false);

    // (weight, node, cameFrom) — lazy-deletion Prim on our MinHeap.
    MinHeap<std::tuple<double, int, int>> pq;

    // outer loop handles a disconnected graph (spanning forest)
    for (int seed = 0; seed < n; ++seed) {
        if (inTree[seed]) continue;
        pq.push({0.0, seed, -1});

        while (!pq.empty()) {
            const auto [w, u, from] = pq.pop();
            if (inTree[u]) continue;             // stale heap entry
            inTree[u] = true;
            if (from != -1) {
                res.edges.push_back({from, u, w, roadBetween(g, from, u)});
                res.total_km += w;
            }
            for (const Edge& e : g.neighbors(u))
                if (!inTree[e.to])               // planning view: include
                    pq.push({e.length_km, e.to, u});  // blocked roads too
        }
    }

    res.runtime_us = std::chrono::duration<double, std::micro>(
                         Clock::now() - t0).count();
    return res;
}

MstResult kruskalMst(const Graph& g) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();

    MstResult res;
    res.algo = "kruskal";
    const int n = g.size();

    // HEAPSORT the edge list with our own MinHeap: push every road once,
    // pop them back in ascending length order.
    MinHeap<std::tuple<double, int, int>> sorted;
    for (int u = 0; u < n; ++u)
        for (const Edge& e : g.neighbors(u))
            if (u < e.to)                        // each undirected road once
                sorted.push({e.length_km, u, e.to});

    UnionFind uf(n);
    while (!sorted.empty() && static_cast<int>(res.edges.size()) < n - 1) {
        const auto [w, u, v] = sorted.pop();
        if (uf.unite(u, v)) {                    // no cycle -> take it
            res.edges.push_back({u, v, w, roadBetween(g, u, v)});
            res.total_km += w;
        }
    }

    res.runtime_us = std::chrono::duration<double, std::micro>(
                         Clock::now() - t0).count();
    return res;
}

}  // namespace lifeline
