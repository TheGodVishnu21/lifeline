#include "simulation/disaster.h"

#include <algorithm>

namespace lifeline {

Disaster simulateDisaster(Graph& g, int epicenter, const std::string& type,
                          int severity) {
    Disaster d;
    d.epicenter = epicenter;
    d.type = type.empty() ? "flood" : type;
    d.severity = std::max(1, std::min(3, severity));
    if (!g.validId(epicenter)) return d;

    // ---- BFS by levels: classic queue, but processed wave-by-wave so
    // the frontend can animate the spread ring by ring.
    std::vector<bool> inZone(g.size(), false);
    std::vector<int> frontier = {epicenter};
    inZone[epicenter] = true;
    d.waves.push_back(frontier);

    for (int depth = 1; depth <= d.severity; ++depth) {
        std::vector<int> next;
        for (int u : frontier)
            for (const Edge& e : g.neighbors(u))
                if (!inZone[e.to]) {
                    inZone[e.to] = true;
                    next.push_back(e.to);
                }
        if (next.empty()) break;
        std::sort(next.begin(), next.end());
        d.waves.push_back(next);
        frontier = std::move(next);
    }

    for (const auto& wave : d.waves)
        d.affected.insert(d.affected.end(), wave.begin(), wave.end());

    // ---- block every road fully inside the zone
    for (int u : d.affected)
        for (const Edge& e : g.neighbors(u))
            if (u < e.to && inZone[e.to]) {
                g.setEdgeBlocked(u, e.to, true);
                d.blocked.push_back({u, e.to});
            }

    return d;
}

void clearDisaster(Graph& g) { g.resetBlockedAll(); }

}  // namespace lifeline
