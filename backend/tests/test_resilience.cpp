// tests/test_resilience.cpp
// Ground truth for the city numbers below was computed independently
// with Python networkx (bridges, articulation_points,
// minimum_spanning_tree) — our C++ must reproduce it exactly.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "core/city_loader.h"
#include "core/graph.h"
#include "ds/trie.h"
#include "resilience/critical.h"
#include "resilience/mst.h"
#include "resilience/union_find.h"
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

static void testUnionFind() {
    UnionFind uf(6);
    CHECK(uf.components() == 6, "UF starts with n components");
    CHECK(uf.unite(0, 1) && uf.unite(1, 2), "UF unite merges");
    CHECK(!uf.unite(0, 2), "UF unite returns false on same set");
    CHECK(uf.connected(0, 2) && !uf.connected(0, 5),
          "UF connected() correct");
    CHECK(uf.components() == 4, "UF component count tracks merges");
}

static void testCriticalKnownGraphs() {
    // chain 0-1-2: both edges are bridges, middle node is the only AP
    Graph chain;
    for (int i = 0; i < 3; ++i) chain.addNode("c", "t", 0, 0);
    chain.addEdge(0, 1, 1, 0, "");
    chain.addEdge(1, 2, 1, 0, "");
    auto r1 = analyzeCritical(chain);
    CHECK(r1.bridges.size() == 2 && r1.articulation_points ==
              std::vector<int>{1},
          "chain: 2 bridges, AP = {1}");

    // triangle: fully cyclic — nothing critical
    Graph tri;
    for (int i = 0; i < 3; ++i) tri.addNode("t", "t", 0, 0);
    tri.addEdge(0, 1, 1, 0, "");
    tri.addEdge(1, 2, 1, 0, "");
    tri.addEdge(2, 0, 1, 0, "");
    auto r2 = analyzeCritical(tri);
    CHECK(r2.bridges.empty() && r2.articulation_points.empty(),
          "triangle: 0 bridges, 0 APs");

    // barbell: two triangles joined by one edge (2-3)
    Graph bar;
    for (int i = 0; i < 6; ++i) bar.addNode("b", "t", 0, 0);
    bar.addEdge(0, 1, 1, 0, ""); bar.addEdge(1, 2, 1, 0, "");
    bar.addEdge(2, 0, 1, 0, ""); bar.addEdge(3, 4, 1, 0, "");
    bar.addEdge(4, 5, 1, 0, ""); bar.addEdge(5, 3, 1, 0, "");
    bar.addEdge(2, 3, 1, 0, "");
    auto r3 = analyzeCritical(bar);
    CHECK(r3.bridges == (std::vector<std::pair<int, int>>{{2, 3}}),
          "barbell: the joining edge is the only bridge");
    CHECK(r3.articulation_points == (std::vector<int>{2, 3}),
          "barbell: its endpoints are the only APs");
    CHECK(r3.components == 1, "barbell: one component");
}

static void testCriticalOnCity() {
    Graph g = loadCityFromJson(cityPath(), nullptr);

    // networkx ground truth: healthy Indrapur has NO single point of
    // failure (min degree 3, meshy by design).
    auto healthy = analyzeCritical(g);
    CHECK(healthy.bridges.empty(), "healthy city: 0 bridges (networkx ✓)");
    CHECK(healthy.articulation_points.empty(),
          "healthy city: 0 articulation points (networkx ✓)");
    CHECK(healthy.components == 1, "healthy city: 1 component");

    // flood sev1 @ 26 — tools/verify_city.py says:
    //   components = 2 (node 26 isolated)
    //   bridges    = (20,21) (23,27)
    //   APs        = 17 20 23 27
    simulateDisaster(g, 26, "flood", 1);
    auto r = analyzeCritical(g);
    CHECK(r.components == 2, "flooded: 2 components");
    CHECK(r.component_members.back() == std::vector<int>{26},
          "flooded: node 26 is the isolated component");
    const std::vector<std::pair<int, int>> expBridges =
        {{20, 21}, {23, 27}};
    CHECK(r.bridges == expBridges,
          "flooded: 2 bridges match the verifier exactly");
    CHECK(r.articulation_points == (std::vector<int>{17, 20, 23, 27}),
          "flooded: 4 articulation points match the verifier exactly");
}

static void testMst() {
    Graph g = loadCityFromJson(cityPath(), nullptr);

    const auto p = primMst(g);
    const auto k = kruskalMst(g);

    // verifier ground truth: 34.169 km, 39 edges
    CHECK(p.edges.size() == 39, "Prim: 39 edges (n-1)");
    CHECK(k.edges.size() == 39, "Kruskal: 39 edges (n-1)");
    CHECK(near(p.total_km, k.total_km),
          "Prim total == Kruskal total (same optimum)");
    CHECK(near(p.total_km, 34.169, 1e-3),
          "MST total == 34.169 km (verifier ✓)");

    // MST must span: every node appears in the edge set
    std::set<int> covered;
    for (const auto& e : p.edges) { covered.insert(e.u); covered.insert(e.v); }
    CHECK(static_cast<int>(covered.size()) == g.size(),
          "Prim MST spans all 40 nodes");

    // both trees must be acyclic + connected — verify with OUR UnionFind
    UnionFind uf(g.size());
    bool acyclic = true;
    for (const auto& e : k.edges)
        if (!uf.unite(e.u, e.v)) acyclic = false;
    CHECK(acyclic && uf.components() == 1,
          "Kruskal edges form a spanning tree (UnionFind verified)");
}

static void testTrie() {
    Graph g = loadCityFromJson(cityPath(), nullptr);
    Trie trie;
    for (const Node& n : g.nodes()) trie.insert(n.name, n.id);
    CHECK(trie.size() == 40, "trie holds all 40 names");

    auto riv = trie.suggest("riv");
    std::set<int> rivSet(riv.begin(), riv.end());
    CHECK(rivSet == std::set<int>({15, 26, 27, 31}),
          "'riv' -> Riverside Colony + Riverside Shelter + both River Bridges");

    CHECK(trie.suggest("RING ROAD").size() == 2,
          "case-insensitive: 'RING ROAD' -> 2 results");
    CHECK(trie.suggest("city hospital") == std::vector<int>{0},
          "full exact name is also a valid prefix");
    CHECK(trie.suggest("xyz").empty(), "unknown prefix -> empty");
    CHECK(trie.suggest("riv", 1).size() == 1, "limit parameter respected");
}

int main() {
    std::printf("--- union-find ---\n");
    testUnionFind();
    std::printf("--- bridges & articulation points (known graphs) ---\n");
    testCriticalKnownGraphs();
    std::printf("--- bridges & articulation points (Indrapur) ---\n");
    testCriticalOnCity();
    std::printf("--- MST: Prim vs Kruskal ---\n");
    testMst();
    std::printf("--- trie autocomplete ---\n");
    testTrie();
    std::printf("\n%s (%d failures)\n",
                g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
