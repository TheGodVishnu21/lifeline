// routing/astar.h
// A* search: Dijkstra + admissible heuristic (straight-line haversine km).
// Explores fewer nodes than Dijkstra while returning the SAME optimal
// distance — the /api/route?algo=compare demo proves this live.
#pragma once

#include "core/graph.h"
#include "routing/route_result.h"

namespace lifeline {

RouteResult astar(const Graph& g, int src, int dst);

}  // namespace lifeline
