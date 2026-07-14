// simulation/disaster.h
// -----------------------------------------------------------------------
// Disaster spread = plain BFS. The epicenter is wave 0; every hop of BFS
// is the disaster front moving outward one "ring" of the road network.
// severity = how many hops the disaster reaches (1..3).
//
// Effect on the city: every road whose BOTH endpoints are inside the
// affected set is blocked (impassable). Routing (Dijkstra/A*) already
// skips blocked edges, so dispatch re-routes automatically.
//
// `type` (flood/fire/earthquake) is cosmetic in Phase 2 — the spread
// model is the same. Later phases can vary spread speed per type.
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/graph.h"

namespace lifeline {

struct Disaster {
    int epicenter = -1;
    std::string type;                          // flood / fire / earthquake
    int severity = 1;                          // BFS radius in hops (1..3)
    std::vector<std::vector<int>> waves;       // waves[0] = {epicenter}
    std::vector<int> affected;                 // all wave nodes, flattened
    std::vector<std::pair<int, int>> blocked;  // roads closed, u < v
};

// Runs BFS from `epicenter` up to `severity` hops and blocks every road
// inside the affected zone (mutates g). severity is clamped to 1..3.
Disaster simulateDisaster(Graph& g, int epicenter, const std::string& type,
                          int severity);

// Reopens all roads.
void clearDisaster(Graph& g);

}  // namespace lifeline
