#include "dispatch/dispatch.h"

#include <chrono>
#include <cmath>

#include "ds/max_heap.h"
#include "routing/dijkstra.h"

namespace lifeline {

bool vehicleservesKind(const std::string& vType, const std::string& kind) {
    if (kind == "any") return true;
    if (kind == "medical") return vType == "ambulance";
    if (kind == "fire")    return vType == "fire_truck";
    if (kind == "police")  return vType == "police";
    return false;
}

DispatchResult dispatch(const Graph& g, std::vector<Incident> incidents,
                        std::vector<Vehicle> vehicles) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    DispatchResult res;

    // --- TRIAGE: load all incidents into a max-heap keyed by severity.
    // Equal severity falls back to insertion order (stable) inside the
    // heap itself, so lower incident id is served first on ties.
    MaxHeap<Incident> triage(
        [](const Incident& i) { return i.severity; });
    for (const Incident& inc : incidents)
        if (g.validId(inc.node)) triage.push(inc);

    // --- GREEDY: serve most-severe first, assign nearest free vehicle.
    while (!triage.empty()) {
        const Incident inc = triage.pop();

        // ONE Dijkstra from the incident gives road distance to every
        // vehicle at once (undirected graph => symmetric distances).
        // Old approach was one Dijkstra per vehicle: O(V_veh) times the
        // work for the same information.
        const AllDistances all = dijkstraAll(g, inc.node);

        int    bestVeh = -1;
        double bestDist = 0.0;

        for (size_t vi = 0; vi < vehicles.size(); ++vi) {
            const Vehicle& v = vehicles[vi];
            if (!v.available) continue;
            if (!vehicleservesKind(v.type, inc.kind)) continue;
            if (!std::isfinite(all.dist[v.node])) continue;  // cut off

            if (bestVeh == -1 || all.dist[v.node] < bestDist) {
                bestVeh = static_cast<int>(vi);
                bestDist = all.dist[v.node];
            }
        }

        if (bestVeh == -1) {                      // nobody can reach it
            res.unassigned.push_back(inc.id);
            continue;
        }

        Vehicle& v = vehicles[bestVeh];
        v.available = false;                      // now busy

        // Walk the parent chain vehicle -> incident. Because the tree is
        // rooted at the incident, this walk IS the vehicle's drive path.
        std::vector<int> path;
        for (int cur = v.node; cur != -1 && cur != inc.node;
             cur = all.parent[cur])
            path.push_back(cur);
        path.push_back(inc.node);

        Assignment a;
        a.incident_id   = inc.id;
        a.severity      = inc.severity;
        a.incident_kind = inc.kind;
        a.incident_node = inc.node;
        a.vehicle_id    = v.id;
        a.vehicle_type  = v.type;
        a.vehicle_node  = v.node;
        a.distance_km   = bestDist;
        a.eta_min       = bestDist / 40.0 * 60.0; // 40 km/h response speed
        a.path          = std::move(path);
        res.total_response_km += bestDist;
        res.assignments.push_back(std::move(a));
    }

    res.runtime_us = std::chrono::duration<double, std::micro>(
                         Clock::now() - t0).count();
    return res;
}

}  // namespace lifeline
