#include "routing/dijkstra.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>
#include <vector>

#include "ds/min_heap.h"

namespace lifeline {

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

// Walk parent[] backwards from dst to src, then reverse.
std::vector<int> reconstructPath(const std::vector<int>& parent,
                                 int src, int dst) {
    std::vector<int> path;
    for (int cur = dst; cur != -1; cur = parent[cur]) path.push_back(cur);
    std::reverse(path.begin(), path.end());
    if (path.empty() || path.front() != src) return {};
    return path;
}
}  // namespace

RouteResult dijkstra(const Graph& g, int src, int dst) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();

    RouteResult res;
    const int n = g.size();
    if (!g.validId(src) || !g.validId(dst)) return res;

    std::vector<double> dist(n, kInf);
    std::vector<int>    parent(n, -1);
    std::vector<bool>   settled(n, false);

    // (distance so far, node) — MinHeap orders by pair::operator<,
    // i.e. smallest distance first.
    MinHeap<std::pair<double, int>> pq;
    dist[src] = 0.0;
    pq.push({0.0, src});

    while (!pq.empty()) {
        const auto [d, u] = pq.pop();
        if (settled[u]) continue;      // lazy deletion: stale entry
        settled[u] = true;
        ++res.nodes_explored;

        if (u == dst) break;           // early exit: destination settled

        for (const Edge& e : g.neighbors(u)) {
            if (e.blocked) continue;   // disasters close roads (Phase 2+)
            const double nd = d + e.length_km;
            if (nd < dist[e.to]) {     // relaxation step
                dist[e.to] = nd;
                parent[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }

    res.runtime_us = std::chrono::duration<double, std::micro>(
                         Clock::now() - t0).count();

    if (dist[dst] == kInf) return res;  // unreachable
    res.found = true;
    res.distance_km = dist[dst];
    res.path = reconstructPath(parent, src, dst);
    return res;
}

AllDistances dijkstraAll(const Graph& g, int src) {
    const int n = g.size();
    AllDistances out;
    out.dist.assign(n, kInf);
    out.parent.assign(n, -1);
    if (!g.validId(src)) return out;

    std::vector<bool> settled(n, false);
    MinHeap<std::pair<double, int>> pq;
    out.dist[src] = 0.0;
    pq.push({0.0, src});

    while (!pq.empty()) {
        const auto [d, u] = pq.pop();
        if (settled[u]) continue;
        settled[u] = true;
        for (const Edge& e : g.neighbors(u)) {
            if (e.blocked) continue;
            const double nd = d + e.length_km;
            if (nd < out.dist[e.to]) {
                out.dist[e.to] = nd;
                out.parent[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }
    return out;
}

}  // namespace lifeline
