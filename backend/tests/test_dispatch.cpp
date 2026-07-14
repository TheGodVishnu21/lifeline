// tests/test_dispatch.cpp
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "core/city_loader.h"
#include "core/graph.h"
#include "dispatch/dispatch.h"
#include "dispatch/supply.h"
#include "ds/hash_map.h"
#include "ds/max_heap.h"
#include "routing/dijkstra.h"
#include "simulation/disaster.h"

using namespace lifeline;

static int g_failures = 0;
#define CHECK(cond, msg)                                    \
    do {                                                    \
        if (cond) std::printf("PASS  %s\n", msg);           \
        else { std::printf("FAIL  %s\n", msg); ++g_failures; } \
    } while (0)

static bool near(double a, double b, double e = 1e-6) {
    return std::fabs(a - b) < e;
}

static void testMaxHeap() {
    MaxHeap<int> h([](const int& x) { return x; });
    for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) h.push(v);
    CHECK(h.peek() == 9, "max-heap peek == 9");
    int prev = 1e9, ok = 1;
    while (!h.empty()) { int x = h.pop(); if (x > prev) ok = 0; prev = x; }
    CHECK(ok, "max-heap pops in non-increasing order");

    // stable ties: same priority => insertion order
    struct Item { int id, prio; };
    MaxHeap<Item> t([](const Item& i) { return i.prio; });
    t.push({10, 5}); t.push({11, 5}); t.push({12, 5});
    CHECK(t.pop().id == 10 && t.pop().id == 11 && t.pop().id == 12,
          "max-heap breaks ties by insertion order (stable)");
}

static void testHashMap() {
    HashMap<int> m;
    m.put("ambulance", 3);
    m.put("fire_truck", 2);
    m.put("ambulance", 5);                 // overwrite
    CHECK(m.getOr("ambulance", 0) == 5, "hashmap overwrite works");
    CHECK(m.getOr("police", -1) == -1, "hashmap missing key -> fallback");
    CHECK(m.contains("fire_truck") && !m.contains("boat"),
          "hashmap contains() correct");
    // force several rehashes
    for (int i = 0; i < 200; ++i) m.put("k" + std::to_string(i), i);
    bool allThere = true;
    for (int i = 0; i < 200; ++i)
        if (m.getOr("k" + std::to_string(i), -1) != i) allThere = false;
    CHECK(allThere && m.size() == 202, "hashmap survives rehash, 202 keys");

    // forEach visits every pair exactly once
    int visits = 0;
    m.forEach([&](const std::string&, int) { ++visits; });
    CHECK(visits == 202, "hashmap forEach visits all 202 pairs");
}

static void testKnapsack() {
    // Classic instance: W=10
    //   A w6 v10, B w3 v5, C w4 v7, D w2 v3
    // optimum = C+A? (w10 v17) vs C+B+D (w9 v15) vs A+D+? ...
    // best is A(6,10)+C(4,7) = w10 v17.
    std::vector<SupplyItem> items = {
        {"A", 6, 10}, {"B", 3, 5}, {"C", 4, 7}, {"D", 2, 3}};
    auto p = planSupplies(items, 10);
    CHECK(p.total_value == 17, "knapsack optimum value == 17");
    CHECK(p.total_weight <= 10, "knapsack respects capacity");
    // verify chosen set really sums to the reported totals
    int wsum = 0, vsum = 0;
    for (auto& it : p.chosen) { wsum += it.weight; vsum += it.value; }
    CHECK(wsum == p.total_weight && vsum == p.total_value,
          "knapsack chosen set matches reported totals");

    // capacity 0 and item bigger than capacity
    CHECK(planSupplies(items, 0).total_value == 0, "knapsack cap 0 -> empty");
    CHECK(planSupplies({{"huge", 100, 50}}, 10).chosen.empty(),
          "knapsack skips oversized item");
}

static std::string cityPath() {
    for (const char* c : {"data/city_graph.json",
                          "backend/data/city_graph.json",
                          "../data/city_graph.json"})
        if (std::filesystem::exists(c)) return c;
    return "";
}

static void testDispatchGreedy() {
    Graph g = loadCityFromJson(cityPath(), nullptr);

    // Two medical incidents, severities 5 and 2; two ambulances.
    // The severity-5 incident must be handled FIRST (triage), and each
    // gets its nearest free ambulance (greedy).
    std::vector<Incident> inc = {
        {1, 17, "medical", 2},   // Lake Town, minor
        {2, 0,  "medical", 5},   // City Hospital area, critical
    };
    std::vector<Vehicle> veh = {
        {100, 0, "ambulance", true},   // at City Hospital
        {101, 2, "ambulance", true},   // at Green Valley Hospital
    };
    auto r = dispatch(g, inc, veh);
    CHECK(r.assignments.size() == 2 && r.unassigned.empty(),
          "dispatch: both incidents assigned");
    CHECK(r.assignments[0].severity == 5,
          "dispatch: most severe served first (triage)");
    CHECK(r.assignments[0].vehicle_id != r.assignments[1].vehicle_id,
          "dispatch: one vehicle per incident");
    // vehicle 100 sits ON node 0 => distance 0 to the critical incident
    CHECK(near(r.assignments[0].distance_km, 0.0),
          "dispatch: nearest vehicle chosen (0 km to co-located incident)");

    // Wrong-type incident with no matching vehicle => unassigned.
    auto r2 = dispatch(g, {{9, 5, "fire", 4}},
                       {{200, 5, "ambulance", true}});
    CHECK(r2.assignments.empty() && r2.unassigned.size() == 1,
          "dispatch: incompatible type -> unassigned");
}

static void testDispatchRespectsDisaster() {
    Graph g = loadCityFromJson(cityPath(), nullptr);
    // Cut the city in half. The raw simulateDisaster() does NOT clear
    // previous blocks (only the server wrapper does), so calling it on
    // both bridge nodes blocks both crossings cumulatively.
    simulateDisaster(g, 26, "flood", 1);   // south bridge zone
    simulateDisaster(g, 27, "flood", 1);   // north bridge zone
    // sanity: with both crossings down, west<->east is severed
    CHECK(!dijkstra(g, 5, 1).found, "both bridges down: city is split");

    // Ambulance on the WEST bank can't reach an incident on the EAST bank.
    std::vector<Incident> inc = {{1, 1, "medical", 5}};   // Sunrise (east)
    std::vector<Vehicle> veh = {{100, 5, "ambulance", true}}; // Police HQ (west)
    auto r = dispatch(g, inc, veh);
    CHECK(r.unassigned.size() == 1,
          "dispatch: incident unreachable across split city -> unassigned");
}

int main() {
    std::printf("--- custom data structures ---\n");
    testMaxHeap();
    testHashMap();
    std::printf("--- knapsack (supply loading) ---\n");
    testKnapsack();
    std::printf("--- dispatch (triage + greedy) ---\n");
    testDispatchGreedy();
    testDispatchRespectsDisaster();
    std::printf("\n%s (%d failures)\n",
                g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
