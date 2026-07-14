// tests/test_evacuation.cpp
// 1) Hand-checked flow networks with known max-flow values.
// 2) Max-flow/min-cut THEOREM verified: sum of cut capacities == flow.
// 3) Disaster BFS on the real city (counts verified by hand).
// 4) The two-bridge story: blocking both river crossings splits Indrapur.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "core/city_loader.h"
#include "core/graph.h"
#include "evacuation/max_flow.h"
#include "routing/dijkstra.h"
#include "simulation/disaster.h"

using namespace lifeline;

static int g_failures = 0;

#define CHECK(cond, msg)                                    \
    do {                                                    \
        if (cond) {                                         \
            std::printf("PASS  %s\n", msg);                 \
        } else {                                            \
            std::printf("FAIL  %s\n", msg);                 \
            ++g_failures;                                   \
        }                                                   \
    } while (0)

static bool near(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) < eps;
}

static double cutSum(const EvacResult& r) {
    double s = 0;
    for (const auto& b : r.bottlenecks) s += b.capacity;
    return s;
}

// Path 0 -(5)- 1 -(3)- 2: flow limited by the middle road.
static void testPathGraph() {
    Graph g;
    for (int i = 0; i < 3; ++i) g.addNode("P" + std::to_string(i), "t", 0, 0);
    g.addEdge(0, 1, 1, 5, "wide");
    g.addEdge(1, 2, 1, 3, "narrow");

    const auto r = computeEvacuation(g, {0}, {2});
    CHECK(near(r.max_flow, 3.0), "path graph: max flow == 3");
    CHECK(r.bottlenecks.size() == 1 && r.bottlenecks[0].road == "narrow",
          "path graph: min cut is the narrow road");
    CHECK(near(cutSum(r), r.max_flow), "path graph: cut sum == max flow");
}

// Square with a weak diagonal:
//   0-1 (10), 0-2 (10), 1-3 (10), 2-3 (10), 1-2 (1)
// Two disjoint 10-wide routes 0->3, diagonal adds nothing: flow = 20.
static void testSquareGraph() {
    Graph g;
    for (int i = 0; i < 4; ++i) g.addNode("S" + std::to_string(i), "t", 0, 0);
    g.addEdge(0, 1, 1, 10, "a");
    g.addEdge(0, 2, 1, 10, "b");
    g.addEdge(1, 3, 1, 10, "c");
    g.addEdge(2, 3, 1, 10, "d");
    g.addEdge(1, 2, 1, 1, "diag");

    const auto r = computeEvacuation(g, {0}, {3});
    CHECK(near(r.max_flow, 20.0), "square graph: max flow == 20");
    CHECK(near(cutSum(r), r.max_flow), "square graph: cut sum == max flow");
    CHECK(r.augmenting_paths >= 2, "square graph: >=2 augmenting paths");
}

static void testDegenerateInputs() {
    Graph g;
    g.addNode("A", "t", 0, 0);
    g.addNode("B", "t", 0, 0);
    g.addEdge(0, 1, 1, 7, "only");

    CHECK(near(computeEvacuation(g, {0}, {}).max_flow, 0.0),
          "no sinks: flow == 0");
    // overlapping id is source-only, leaving no sink
    CHECK(near(computeEvacuation(g, {0, 1}, {1}).max_flow, 0.0),
          "sink inside source set: flow == 0");
}

static std::string cityPath() {
    for (const char* c : {"data/city_graph.json",
                          "backend/data/city_graph.json",
                          "../data/city_graph.json"})
        if (std::filesystem::exists(c)) return c;
    return "";
}

static void testDisasterOnCity() {
    Graph g = loadCityFromJson(cityPath(), nullptr);

    // Severity-1 flood at River Bridge South (26). Hand-verified:
    // neighbours of 26 = {15, 25, 8, 21, 11} -> 6 affected nodes.
    // Roads inside the zone: 5 incident to 26 + (15-25),(21-11),(11-8) = 8.
    Disaster d = simulateDisaster(g, 26, "flood", 1);
    CHECK(d.affected.size() == 6, "flood@26 sev1: 6 zones affected");
    CHECK(d.waves.size() == 2 && d.waves[0].size() == 1,
          "flood@26 sev1: 2 waves, wave0 = epicenter");
    CHECK(d.blocked.size() == 8, "flood@26 sev1: 8 roads blocked");

    // West -> East must reroute over the NORTH bridge (node 27) now.
    const auto r = dijkstra(g, 15, 1);
    bool via27 = false;
    for (int id : r.path) via27 |= (id == 27);
    CHECK(r.found, "route 15->1 still exists after flood");
    CHECK(via27, "route 15->1 now crosses North Bridge (27)");
    CHECK(r.distance_km > 4.79, "detour is longer than the 4.78 km original");

    // Block the north crossing too -> the river splits the city.
    simulateDisaster(g, 27, "flood", 1);
    CHECK(!dijkstra(g, 15, 1).found,
          "both bridges down: west->east route impossible (city split)");

    clearDisaster(g);
    CHECK(dijkstra(g, 15, 1).found && g.blockedEdges().empty(),
          "reset reopens every road");
}

static void testEvacuationOnCity() {
    Graph g = loadCityFromJson(cityPath(), nullptr);
    Disaster d = simulateDisaster(g, 26, "flood", 1);

    // Safe shelters = shelters outside the zone: 9 and 10 (11 is inside).
    const auto r = computeEvacuation(g, d.affected, {9, 10, 11});
    CHECK(r.sinks.size() == 2, "shelter 11 excluded (inside the zone)");
    CHECK(r.max_flow > 0, "evacuation capacity > 0");
    CHECK(!r.bottlenecks.empty(), "bottleneck roads reported");
    CHECK(near(cutSum(r), r.max_flow),
          "CITY: min-cut capacity sum == max flow (theorem holds)");
    for (const auto& b : r.bottlenecks)
        if (b.road.empty()) { CHECK(false, "cut contains a super arc"); return; }
    CHECK(true, "min cut contains only real roads");
}

int main() {
    std::printf("--- known flow networks ---\n");
    testPathGraph();
    testSquareGraph();
    testDegenerateInputs();
    std::printf("--- disaster on Indrapur ---\n");
    testDisasterOnCity();
    std::printf("--- evacuation on Indrapur ---\n");
    testEvacuationOnCity();
    std::printf("\n%s (%d failures)\n",
                g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
