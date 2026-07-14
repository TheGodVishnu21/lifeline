// dispatch/dispatch.h
// -----------------------------------------------------------------------
// Emergency dispatch: match incidents to the best-placed vehicles.
//
// Two DSA ideas working together:
//   1. TRIAGE  — a custom MAX-heap (ds/max_heap.h) orders incidents by
//      severity (5 = critical first). Equal severity => arrival order.
//   2. GREEDY ASSIGNMENT — pop the most severe incident, then pick the
//      nearest AVAILABLE compatible vehicle by real road distance
//      (Dijkstra from routing/). Locally-optimal each step = the classic
//      greedy strategy; we log the choice so the viva can discuss where
//      greedy is/ isn't globally optimal.
//
// A vehicle serves one incident at a time (becomes unavailable). Incidents
// with no compatible free vehicle are reported as unassigned.
//
// Blocked roads (from an active disaster) are respected automatically,
// because distances come from the same Dijkstra that skips blocked edges.
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

#include "core/graph.h"

namespace lifeline {

// What kind of responder an incident needs. "any" matches all.
struct Incident {
    int         id = 0;
    int         node = -1;        // location on the graph
    std::string kind;             // "medical" | "fire" | "police" | "any"
    int         severity = 1;     // 1..5, higher = more urgent
};

struct Vehicle {
    int         id = 0;
    int         node = -1;        // current location (usually a station)
    std::string type;             // "ambulance" | "fire_truck" | "police"
    bool        available = true;
};

struct Assignment {
    int    incident_id;
    int    severity;
    std::string incident_kind;
    int    incident_node;
    int    vehicle_id;
    std::string vehicle_type;
    int    vehicle_node;
    double distance_km;           // road distance vehicle -> incident
    double eta_min;               // at 40 km/h response speed
    std::vector<int> path;        // node ids
};

struct DispatchResult {
    std::vector<Assignment> assignments;   // in service order (most severe first)
    std::vector<int>        unassigned;    // incident ids with no vehicle
    double total_response_km = 0.0;
    double runtime_us = 0.0;
};

// Which vehicle type satisfies which incident kind.
bool vehicleservesKind(const std::string& vType, const std::string& kind);

DispatchResult dispatch(const Graph& g, std::vector<Incident> incidents,
                        std::vector<Vehicle> vehicles);

}  // namespace lifeline
