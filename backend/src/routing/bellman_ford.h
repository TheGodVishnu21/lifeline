// routing/bellman_ford.h
// -----------------------------------------------------------------------
// BELLMAN-FORD: single-source shortest paths by brute-force relaxation.
//
// Relax EVERY edge, up to V-1 times. No heap, no heuristic, no early
// exit at the destination — just repeated sweeps until nothing improves.
//
// Why keep it when Dijkstra is faster?
//   1. It works with NEGATIVE edge weights, where Dijkstra's greedy
//      "settled" assumption breaks.
//   2. Its per-edge relaxation structure distributes naturally —
//      distance-vector routing protocols (RIP) are Bellman-Ford.
//   3. It is the honest baseline: same answer as Dijkstra/A*, far more
//      work. That contrast is the point of the Algorithm Lab.
//
// Road lengths are never negative in LifeLine, so the negative-cycle
// detection pass is deliberately omitted (documented in the .cpp) —
// a good viva talking point.
//
// Complexity O(V * E). nodes_explored in the result counts SUCCESSFUL
// relaxations, making "work done" comparable across algorithms.
// -----------------------------------------------------------------------
#pragma once

#include "core/graph.h"
#include "routing/route_result.h"

namespace lifeline {

RouteResult bellmanFord(const Graph& g, int src, int dst);

}  // namespace lifeline
