// resilience/critical.h
// -----------------------------------------------------------------------
// Critical-infrastructure analysis via TARJAN'S single-DFS algorithm.
//
//   BRIDGE            = a road whose failure disconnects the network
//   ARTICULATION PT   = a junction whose failure disconnects the network
//
// One DFS computes both, using discovery times and low-links:
//   disc[u] = when DFS first reached u
//   low[u]  = the earliest-discovered node reachable from u's subtree
//             using at most one back-edge
//   edge (u,v) is a bridge        iff low[v] >  disc[u]
//   u (non-root) is articulation  iff low[v] >= disc[u] for some child v
//   the DFS root is articulation  iff it has 2+ DFS children
//
// Runs on the CURRENT road state — blocked roads are excluded, so this
// answers "after the disaster, which single failure would split the
// city further?". On the healthy Indrapur mesh the answer is NONE
// (0 bridges, 0 articulation points) — by design, every node has
// degree >= 3. Disasters change that dramatically.
//
// Complexity: O(V + E), one pass.
// -----------------------------------------------------------------------
#pragma once

#include <utility>
#include <vector>

#include "core/graph.h"

namespace lifeline {

struct CriticalReport {
    std::vector<std::pair<int, int>> bridges;   // (u, v) with u < v, sorted
    std::vector<int> articulation_points;       // sorted node ids
    int components = 0;                         // over unblocked roads
    std::vector<std::vector<int>> component_members;  // largest first
};

CriticalReport analyzeCritical(const Graph& g);

}  // namespace lifeline
