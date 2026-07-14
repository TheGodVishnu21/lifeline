// routing/floyd_warshall.h
// -----------------------------------------------------------------------
// Floyd-Warshall: ALL-PAIRS shortest paths by dynamic programming.
//
//   dp recurrence: allowing node k as an intermediate,
//     dist[i][j] = min(dist[i][j], dist[i][k] + dist[k][j])
//   Triple loop over k, i, j  =>  O(V^3) time, O(V^2) space.
//
// For a 28-node city that is ~22k inner steps — microseconds — and the
// payoff is a COMPLETE distance matrix: city diameter, average trip
// length, "most central location" (minimum eccentricity), and O(1)
// station-to-station lookups. `next[i][j]` (the first hop from i toward
// j) reconstructs any path without re-running anything.
//
// Runs on the CURRENT road state (blocked roads excluded), so the same
// analytics show how a disaster degrades the whole network at once.
// -----------------------------------------------------------------------
#pragma once

#include <vector>

#include "core/graph.h"

namespace lifeline {

struct AllPairs {
    int n = 0;
    std::vector<std::vector<double>> dist;   // dist[i][j], +inf unreachable
    std::vector<std::vector<int>>    next;   // first hop i -> j, -1 if none
    double runtime_us = 0.0;

    // Rebuild the full node path i -> j from the next matrix. Empty if
    // unreachable.
    std::vector<int> path(int i, int j) const;
};

AllPairs floydWarshall(const Graph& g);

// City-wide analytics derived from the matrix (finite pairs only).
struct CityStats {
    double diameter_km = 0.0;        // longest shortest path
    int    diameter_from = -1, diameter_to = -1;
    double avg_distance_km = 0.0;    // mean over reachable ordered pairs
    int    most_central = -1;        // min eccentricity
    double central_ecc_km = 0.0;
    int    least_central = -1;       // max eccentricity
    double remote_ecc_km = 0.0;
    long long disconnected_pairs = 0;  // unordered pairs with no route
};

CityStats computeCityStats(const AllPairs& ap);

}  // namespace lifeline
