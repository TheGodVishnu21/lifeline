#include "routing/astar.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>
#include <vector>

#include "ds/min_heap.h"

namespace lifeline {

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

std::vector<int> reconstructPath(const std::vector<int>& parent,
                                 int src, int dst) {
    std::vector<int> path;
    for (int cur = dst; cur != -1; cur = parent[cur]) path.push_back(cur);
    std::reverse(path.begin(), path.end());
    if (path.empty() || path.front() != src) return {};
    return path;
}
}  // namespace

RouteResult astar(const Graph& g, int src, int dst) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();

    RouteResult res;
    const int n = g.size();
    if (!g.validId(src) || !g.validId(dst)) return res;

    // gScore = best known cost from src; f = g + h drives the heap.
    // h = haversine straight-line km. Every road length >= straight line
    // (enforced by the data generator), so h never overestimates
    // => admissible => A* result is optimal.
    std::vector<double> gScore(n, kInf);
    std::vector<int>    parent(n, -1);
    std::vector<bool>   settled(n, false);

    MinHeap<std::pair<double, int>> pq;  // (f, node)
    gScore[src] = 0.0;
    pq.push({g.heuristicKm(src, dst), src});

    while (!pq.empty()) {
        const auto [f, u] = pq.pop();
        (void)f;
        if (settled[u]) continue;
        settled[u] = true;
        ++res.nodes_explored;

        if (u == dst) break;  // goal settled => optimal path found

        for (const Edge& e : g.neighbors(u)) {
            if (e.blocked) continue;
            const double ng = gScore[u] + e.length_km;
            if (ng < gScore[e.to]) {
                gScore[e.to] = ng;
                parent[e.to] = u;
                pq.push({ng + g.heuristicKm(e.to, dst), e.to});
            }
        }
    }

    res.runtime_us = std::chrono::duration<double, std::micro>(
                         Clock::now() - t0).count();

    if (gScore[dst] == kInf) return res;
    res.found = true;
    res.distance_km = gScore[dst];
    res.path = reconstructPath(parent, src, dst);
    return res;
}

}  // namespace lifeline
