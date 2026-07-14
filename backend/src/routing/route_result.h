// routing/route_result.h
// Common result type returned by every routing algorithm, so the API
// layer and the console UI can treat Dijkstra / A* identically.
#pragma once

#include <vector>

namespace lifeline {

struct RouteResult {
    bool             found = false;
    std::vector<int> path;               // node ids, src ... dst
    double           distance_km = 0.0;
    long long        nodes_explored = 0; // settled nodes (A* vs Dijkstra demo)
    double           runtime_us = 0.0;   // wall-clock microseconds
};

}  // namespace lifeline
