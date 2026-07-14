// tests/test_analytics.cpp
// Bellman-Ford and Floyd-Warshall must agree with Dijkstra everywhere,
// and the city stats must match networkx ground truth:
//   diameter 8.114 km (Hill View Colony <-> City Mall)
//   average  3.993 km, most central = Bus Terminal (ecc 4.643)
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "core/city_loader.h"
#include "core/graph.h"
#include "routing/bellman_ford.h"
#include "routing/dijkstra.h"
#include "routing/floyd_warshall.h"
#include "simulation/disaster.h"

using namespace lifeline;

static int g_failures = 0;
#define CHECK(cond, msg)                                       \
    do {                                                       \
        if (cond) std::printf("PASS  %s\n", msg);              \
        else { std::printf("FAIL  %s\n", msg); ++g_failures; } \
    } while (0)

static bool near(double a, double b, double e = 1e-6) {
    return std::fabs(a - b) < e;
}

static std::string cityPath() {
    for (const char* c : {"data/city_graph.json",
                          "backend/data/city_graph.json",
                          "../data/city_graph.json"})
        if (std::filesystem::exists(c)) return c;
    return "";
}

static void testBellmanFordKnown() {
    // same paper graph as test_routing: shortest 0->4 = 7 via 0-2-1-3-4
    Graph g;
    for (int i = 0; i < 5; ++i) g.addNode("N", "t", 0, 0);
    g.addEdge(0, 1, 4, 0, ""); g.addEdge(0, 2, 1, 0, "");
    g.addEdge(2, 1, 2, 0, ""); g.addEdge(1, 3, 1, 0, "");
    g.addEdge(2, 3, 5, 0, ""); g.addEdge(3, 4, 3, 0, "");
    g.addEdge(1, 4, 6, 0, "");

    const auto r = bellmanFord(g, 0, 4);
    CHECK(r.found && near(r.distance_km, 7.0), "BF 0->4 == 7 (paper graph)");
    CHECK(r.path.size() == 5, "BF reconstructs the 5-node path");
    CHECK(!bellmanFord(g, 0, 0).path.empty(), "BF src==dst handled");
}

static void testBellmanFordVsDijkstra() {
    Graph g = loadCityFromJson(cityPath(), nullptr);
    bool allEqual = true;
    for (int s = 0; s < g.size(); ++s)
        for (int t = 0; t < g.size(); ++t) {
            const auto d = dijkstra(g, s, t);
            const auto b = bellmanFord(g, s, t);
            if (d.found != b.found || !near(d.distance_km, b.distance_km))
                allEqual = false;
        }
    CHECK(allEqual, "BF == Dijkstra on ALL 784 city pairs");

    // and with a disaster active (blocked edges respected)
    simulateDisaster(g, 26, "flood", 1);
    const auto d = dijkstra(g, 15, 1);
    const auto b = bellmanFord(g, 15, 1);
    CHECK(d.found && b.found && near(d.distance_km, b.distance_km),
          "BF == Dijkstra during a disaster too");
}

static void testFloydWarshall() {
    Graph g = loadCityFromJson(cityPath(), nullptr);
    const AllPairs ap = floydWarshall(g);

    // full agreement with Dijkstra from every source
    bool allEqual = true;
    for (int s = 0; s < g.size(); ++s) {
        const AllDistances one = dijkstraAll(g, s);
        for (int t = 0; t < g.size(); ++t)
            if (!near(one.dist[t], ap.dist[s][t])) allEqual = false;
    }
    CHECK(allEqual, "FW matrix == Dijkstra from every source (784 pairs)");

    // path reconstruction via next matrix
    const auto p = ap.path(15, 1);
    double sum = 0.0;
    bool chainOk = !p.empty() && p.front() == 15 && p.back() == 1;
    for (size_t i = 0; chainOk && i + 1 < p.size(); ++i) {
        bool edgeFound = false;
        for (const Edge& e : g.neighbors(p[i]))
            if (e.to == p[i + 1]) { sum += e.length_km; edgeFound = true; }
        if (!edgeFound) chainOk = false;
    }
    CHECK(chainOk && near(sum, ap.dist[15][1]),
          "FW next-matrix path 15->1 is a real road chain, length matches");

    // networkx ground truth
    const CityStats st = computeCityStats(ap);
    CHECK(near(st.diameter_km, 8.114, 1e-3),
          "diameter == 8.114 km (networkx ✓)");
    CHECK((st.diameter_from == 16 && st.diameter_to == 22) ||
          (st.diameter_from == 22 && st.diameter_to == 16),
          "diameter pair == Hill View Colony <-> City Mall");
    CHECK(near(st.avg_distance_km, 3.993, 1e-3),
          "average distance == 3.993 km (networkx ✓)");
    CHECK(st.most_central == 8 && near(st.central_ecc_km, 4.643, 1e-3),
          "most central == Bus Terminal, ecc 4.643 (networkx ✓)");
    CHECK(st.disconnected_pairs == 0, "healthy city: 0 disconnected pairs");
}

static void testFloydWarshallDisaster() {
    Graph g = loadCityFromJson(cityPath(), nullptr);
    // block both crossings -> west/east split
    simulateDisaster(g, 26, "flood", 1);
    simulateDisaster(g, 27, "flood", 1);
    const AllPairs ap = floydWarshall(g);
    CHECK(std::isinf(ap.dist[5][1]),
          "city split: FW reports Police HQ -> Sunrise Hospital unreachable");
    const CityStats st = computeCityStats(ap);
    CHECK(st.disconnected_pairs > 0,
          "city split: disconnected pair count is nonzero");
}

int main() {
    std::printf("--- bellman-ford ---\n");
    testBellmanFordKnown();
    testBellmanFordVsDijkstra();
    std::printf("--- floyd-warshall ---\n");
    testFloydWarshall();
    testFloydWarshallDisaster();
    std::printf("\n%s (%d failures)\n",
                g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
