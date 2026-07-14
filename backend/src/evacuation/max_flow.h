// evacuation/max_flow.h
// -----------------------------------------------------------------------
// Evacuation capacity = MAXIMUM FLOW through the road network.
//
//   sources = nodes inside the disaster zone (people to move out)
//   sinks   = safe shelters (capacity to receive them)
//
// We attach a super-source S to every source and every sink to a
// super-sink T with infinite capacity, then run Ford-Fulkerson with BFS
// augmenting paths (= Edmonds-Karp, O(V * E^2)). Road capacities are in
// people/hour, so max flow answers the question the city actually asks:
// "kitne log per hour nikal sakte hain?"
//
// Min cut comes free after max flow: BFS the residual graph from S; every
// saturated road crossing from the reachable side to the unreachable side
// is a BOTTLENECK. Max-flow == sum of min-cut capacities (max-flow/min-cut
// theorem — the unit tests assert this equality on every run).
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

#include "core/graph.h"

namespace lifeline {

struct Bottleneck {
    int from = -1, to = -1;
    double capacity = 0.0;   // people per hour
    std::string road;
};

struct EvacResult {
    double max_flow = 0.0;   // people per hour, danger zone -> shelters
    std::vector<int> sources, sinks;
    std::vector<Bottleneck> bottlenecks;   // the min cut
    long long augmenting_paths = 0;
    double runtime_us = 0.0;
};

// Blocked roads are excluded from the network. Any id appearing in both
// lists is treated as a source only. Empty sources or sinks => flow 0.
EvacResult computeEvacuation(const Graph& g, std::vector<int> sources,
                             std::vector<int> sinks);

}  // namespace lifeline
