// tests/test_routing.cpp
// 1) Hand-checked tiny graph: exact shortest distances known on paper.
// 2) Real city: A* must return the SAME distance as Dijkstra for every
//    sampled pair (optimality check), and the city must be connected.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "core/city_loader.h"
#include "core/graph.h"
#include "routing/astar.h"
#include "routing/dijkstra.h"

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

// Paper-verified graph (all coords 0 => A* heuristic 0 => behaves like
// Dijkstra, so both must produce these exact numbers):
//
//   0-1 (4), 0-2 (1), 2-1 (2), 1-3 (1), 2-3 (5), 3-4 (3), 1-4 (6)
//   shortest 0->1 = 3 (0-2-1)
//   shortest 0->3 = 4 (0-2-1-3)
//   shortest 0->4 = 7 (0-2-1-3-4)
static void testKnownGraph() {
    Graph g;
    for (int i = 0; i < 5; ++i)
        g.addNode("N" + std::to_string(i), "test", 0.0, 0.0);
    g.addEdge(0, 1, 4, 0, "");
    g.addEdge(0, 2, 1, 0, "");
    g.addEdge(2, 1, 2, 0, "");
    g.addEdge(1, 3, 1, 0, "");
    g.addEdge(2, 3, 5, 0, "");
    g.addEdge(3, 4, 3, 0, "");
    g.addEdge(1, 4, 6, 0, "");

    auto d = dijkstra(g, 0, 4);
    CHECK(d.found && near(d.distance_km, 7.0), "dijkstra 0->4 == 7");
    CHECK(d.path.size() == 5 && d.path.front() == 0 && d.path.back() == 4,
          "dijkstra 0->4 path is 5 nodes 0..4");

    auto a = astar(g, 0, 4);
    CHECK(a.found && near(a.distance_km, 7.0), "astar    0->4 == 7");

    CHECK(near(dijkstra(g, 0, 3).distance_km, 4.0), "dijkstra 0->3 == 4");
    CHECK(near(dijkstra(g, 0, 1).distance_km, 3.0), "dijkstra 0->1 == 3");

    auto same = dijkstra(g, 2, 2);
    CHECK(same.found && near(same.distance_km, 0.0) && same.path.size() == 1,
          "src == dst handled");
}

static void testRealCity() {
    std::string path;
    for (const char* c : {"data/city_graph.json",
                          "backend/data/city_graph.json",
                          "../data/city_graph.json"})
        if (std::filesystem::exists(c)) { path = c; break; }
    if (path.empty()) {
        std::printf("FAIL  city_graph.json not found\n");
        ++g_failures;
        return;
    }

    Graph g = loadCityFromJson(path, nullptr);
    CHECK(g.size() == 28, "city has 28 nodes");
    CHECK(g.edgeCount() == 48, "city has 48 edges");

    bool allFound = true, allEqual = true, aStarNeverWorse = true;
    for (int s = 0; s < g.size(); ++s) {
        for (int t = 0; t < g.size(); ++t) {
            const auto d = dijkstra(g, s, t);
            const auto a = astar(g, s, t);
            if (!d.found || !a.found) allFound = false;
            if (!near(d.distance_km, a.distance_km)) allEqual = false;
            if (a.nodes_explored > d.nodes_explored) aStarNeverWorse = false;
        }
    }
    CHECK(allFound, "all 28x28 pairs reachable (city connected)");
    CHECK(allEqual, "A* distance == Dijkstra distance on ALL pairs (optimal)");
    CHECK(aStarNeverWorse, "A* never explores more nodes than Dijkstra");

    // name lookup
    CHECK(g.findByName("city hospital") == 0, "case-insensitive name lookup");
}

int main() {
    std::printf("--- known graph ---\n");
    testKnownGraph();
    std::printf("--- real city ---\n");
    testRealCity();
    std::printf("\n%s (%d failures)\n",
                g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
