#include "routing/bellman_ford.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

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

RouteResult bellmanFord(const Graph& g, int src, int dst) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();

    RouteResult res;
    const int n = g.size();
    if (!g.validId(src) || !g.validId(dst)) return res;

    std::vector<double> dist(n, kInf);
    std::vector<int>    parent(n, -1);
    dist[src] = 0.0;

    // V-1 passes upper bound; early exit when a pass changes nothing.
    for (int pass = 0; pass < n - 1; ++pass) {
        bool changed = false;
        for (int u = 0; u < n; ++u) {
            if (dist[u] == kInf) continue;
            for (const Edge& e : g.neighbors(u)) {
                if (e.blocked) continue;
                const double nd = dist[u] + e.length_km;
                if (nd < dist[e.to]) {           // relaxation
                    dist[e.to] = nd;
                    parent[e.to] = u;
                    ++res.nodes_explored;        // = successful relaxations
                    changed = true;
                }
            }
        }
        if (!changed) break;                     // all settled early
    }
    // (negative-cycle pass omitted: road lengths are always positive)

    res.runtime_us = std::chrono::duration<double, std::micro>(
                         Clock::now() - t0).count();

    if (dist[dst] == kInf) return res;
    res.found = true;
    res.distance_km = dist[dst];
    res.path = reconstructPath(parent, src, dst);
    return res;
}

}  // namespace lifeline
