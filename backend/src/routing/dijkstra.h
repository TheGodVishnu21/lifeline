// routing/dijkstra.h
// Single-source shortest path on non-negative edge weights.
// Complexity: O((V + E) log V) with a binary heap.
#pragma once

#include <vector>

#include "core/graph.h"
#include "routing/route_result.h"

namespace lifeline {

RouteResult dijkstra(const Graph& g, int src, int dst);

// Single-source, ALL destinations (no early exit). Dispatch uses this:
// one Dijkstra from the incident gives the road distance to EVERY
// vehicle at once, instead of one Dijkstra per vehicle.
// dist[v] is +infinity when v is unreachable.
struct AllDistances {
    std::vector<double> dist;
    std::vector<int>    parent;   // parent[src] == -1
};
AllDistances dijkstraAll(const Graph& g, int src);

}  // namespace lifeline
