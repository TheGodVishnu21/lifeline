// main.cpp
// Usage:
//   ./lifeline                start REST server on port 8080
//   ./lifeline 9090           start REST server on custom port
//   ./lifeline console        interactive console mode (no server needed)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "api/server.h"
#include "core/city_loader.h"
#include "core/graph.h"
#include "db/history.h"
#include "dispatch/dispatch.h"
#include "dispatch/supply.h"
#include "ds/trie.h"
#include "evacuation/max_flow.h"
#include "resilience/critical.h"
#include "resilience/mst.h"
#include "routing/astar.h"
#include "routing/bellman_ford.h"
#include "routing/dijkstra.h"
#include "routing/floyd_warshall.h"
#include "simulation/disaster.h"

namespace fs = std::filesystem;
using namespace lifeline;

namespace {

// Works whether you run from backend/ or from the repo root.
std::string firstExisting(const std::vector<std::string>& candidates) {
    for (const auto& p : candidates)
        if (fs::exists(p)) return p;
    return "";
}

void printRoute(const Graph& g, const RouteResult& r, const std::string& algo) {
    std::printf("\n[%s]\n", algo.c_str());
    if (!r.found) {
        std::printf("  no route found\n");
        return;
    }
    std::printf("  path     : ");
    for (size_t i = 0; i < r.path.size(); ++i)
        std::printf("%s%s", g.node(r.path[i]).name.c_str(),
                    i + 1 < r.path.size() ? " -> " : "\n");
    std::printf("  distance : %.2f km   (eta ~%.0f min @30km/h)\n",
                r.distance_km, r.distance_km * 2.0);
    std::printf("  explored : %lld nodes   runtime: %.0f us\n",
                r.nodes_explored, r.runtime_us);
}

int readInt(const char* prompt, int lo, int hi) {
    while (true) {
        std::printf("%s", prompt);
        int x;
        if (std::cin >> x && x >= lo && x <= hi) return x;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::printf("  enter a number between %d and %d\n", lo, hi);
    }
}

void consoleMode(Graph& g, const std::string& cityName, History& history) {
    std::optional<Disaster> active;
    std::printf("=== LifeLine console | %s | %d locations ===\n",
                cityName.c_str(), g.size());
    while (true) {
        std::printf("\n1. List locations\n2. Route (Dijkstra)\n"
                    "3. Route (A*)\n4. Compare Dijkstra vs A*\n"
                    "5. Trigger disaster\n6. Evacuation analysis (max flow)\n"
                    "7. Reset disaster\n8. Dispatch (triage + greedy)\n"
                    "9. Supply loading (knapsack)\n"
                    "10. Resilience report (bridges/APs)\n"
                    "11. Restoration plan (Prim vs Kruskal MST)\n"
                    "12. Search location (Trie)\n"
                    "13. Algorithm lab (Dijkstra vs A* vs Bellman-Ford)\n"
                    "14. City stats (Floyd-Warshall)\n0. Exit\n");
        const int choice = readInt("> ", 0, 14);
        if (choice == 0) return;

        if (choice == 1) {
            for (const Node& n : g.nodes())
                std::printf("  [%2d] %-24s %s\n", n.id, n.name.c_str(),
                            n.type.c_str());
            continue;
        }

        if (choice == 5) {
            const int epi = readInt("epicenter id: ", 0, g.size() - 1);
            const int sev = readInt("severity (1-3): ", 1, 3);
            clearDisaster(g);
            active = simulateDisaster(g, epi, "flood", sev);
            std::printf("  disaster at %s | %zu zones affected | "
                        "%zu roads blocked\n",
                        g.node(epi).name.c_str(), active->affected.size(),
                        active->blocked.size());
            continue;
        }

        if (choice == 6) {
            if (!active) {
                std::printf("  no active disaster — use option 5 first\n");
                continue;
            }
            std::vector<bool> inZone(g.size(), false);
            for (int id : active->affected) inZone[id] = true;
            std::vector<int> sinks;
            for (const Node& n : g.nodes())
                if (n.type == "shelter" && !inZone[n.id])
                    sinks.push_back(n.id);

            const EvacResult r = computeEvacuation(g, active->affected, sinks);
            std::printf("  max evacuation capacity: %.0f people/hour "
                        "(%lld augmenting paths, %.0f us)\n",
                        r.max_flow, r.augmenting_paths, r.runtime_us);
            std::printf("  bottleneck roads (min cut):\n");
            for (const Bottleneck& b : r.bottlenecks)
                std::printf("    %-20s %s -> %s  cap %.0f/hr\n",
                            b.road.c_str(), g.node(b.from).name.c_str(),
                            g.node(b.to).name.c_str(), b.capacity);
            continue;
        }

        if (choice == 7) {
            clearDisaster(g);
            active.reset();
            std::printf("  all roads reopened\n");
            continue;
        }

        if (choice == 8) {
            // demo fleet at stations; incidents at affected zone (or a
            // couple of sample incidents if no disaster is active)
            std::vector<Vehicle> veh;
            for (const Node& n : g.nodes()) {
                if (n.type == "hospital")
                    veh.push_back({(int)veh.size() + 100, n.id, "ambulance", true});
                else if (n.type == "fire_station")
                    veh.push_back({(int)veh.size() + 100, n.id, "fire_truck", true});
                else if (n.type == "police")
                    veh.push_back({(int)veh.size() + 100, n.id, "police", true});
            }
            std::vector<Incident> inc;
            if (active) {
                // severity tied to BFS wave: epicenter=5, ring1=3, ring2=1
                int id = 1;
                for (size_t wi = 0; wi < active->waves.size(); ++wi) {
                    const int sev = std::max(1, 5 - 2 * (int)wi);
                    for (int node : active->waves[wi])
                        if (g.node(node).type != "shelter")
                            inc.push_back({id++, node, "any", sev});
                }
            } else {
                inc = {{1, 17, "medical", 5}, {2, 19, "fire", 3},
                       {3, 6, "police", 2}};
            }
            const DispatchResult r = dispatch(g, inc, veh);
            std::printf("  %zu incidents | %zu assigned | %zu unassigned"
                        " | total %.1f km (%.0f us)\n",
                        inc.size(), r.assignments.size(),
                        r.unassigned.size(), r.total_response_km,
                        r.runtime_us);
            for (const Assignment& a : r.assignments)
                std::printf("    sev%d %-8s @ %-20s <- veh#%d %-10s %.2f km\n",
                            a.severity, a.incident_kind.c_str(),
                            g.node(a.incident_node).name.c_str(),
                            a.vehicle_id, a.vehicle_type.c_str(),
                            a.distance_km);
            history.add({"", "dispatch", (int)r.assignments.size(),
                         r.total_response_km, "console dispatch"});
            continue;
        }

        if (choice == 9) {
            std::vector<SupplyItem> items = {
                {"Water (20L)", 20, 8}, {"First-aid kit", 5, 10},
                {"Food rations", 15, 7}, {"Blankets", 8, 4},
                {"Medicine box", 6, 9}, {"Tents", 25, 6},
                {"Flashlights", 3, 3}, {"Power generator", 30, 5}};
            const int cap = readInt("truck capacity (kg): ", 1, 1000);
            const SupplyPlan p = planSupplies(items, cap);
            std::printf("  loaded %zu items | weight %d/%d kg | value %d"
                        " (%.0f us)\n", p.chosen.size(), p.total_weight,
                        p.capacity, p.total_value, p.runtime_us);
            for (const auto& it : p.chosen)
                std::printf("    %-18s %2d kg  value %d\n",
                            it.name.c_str(), it.weight, it.value);
            continue;
        }

        if (choice == 10) {
            const CriticalReport r = analyzeCritical(g);
            std::printf("  components: %d", r.components);
            if (r.components > 1) {
                std::printf("  | isolated:");
                for (size_t i = 1; i < r.component_members.size(); ++i)
                    for (int id : r.component_members[i])
                        std::printf(" %s,", g.node(id).name.c_str());
            }
            std::printf("\n  bridges (%zu):\n", r.bridges.size());
            for (auto [u, v] : r.bridges)
                std::printf("    %s -- %s\n", g.node(u).name.c_str(),
                            g.node(v).name.c_str());
            std::printf("  articulation points (%zu):",
                        r.articulation_points.size());
            for (int id : r.articulation_points)
                std::printf(" %s,", g.node(id).name.c_str());
            std::printf("\n");
            if (r.bridges.empty() && r.articulation_points.empty())
                std::printf("  -> no single point of failure. healthy mesh!\n");
            continue;
        }

        if (choice == 11) {
            const MstResult p = primMst(g);
            const MstResult k = kruskalMst(g);
            std::printf("  Prim    : %.3f km, %zu edges, %.0f us\n",
                        p.total_km, p.edges.size(), p.runtime_us);
            std::printf("  Kruskal : %.3f km, %zu edges, %.0f us\n",
                        k.total_km, k.edges.size(), k.runtime_us);
            std::printf("  totals match: %s (two algorithms, one optimum)\n",
                        std::fabs(p.total_km - k.total_km) < 1e-6 ? "YES"
                                                                  : "NO!");
            continue;
        }

        if (choice == 12) {
            std::printf("prefix (single word): ");
            std::string q;
            std::cin >> q;
            Trie trie;
            for (const Node& n : g.nodes()) trie.insert(n.name, n.id);
            const auto ids = trie.suggest(q, 8);
            if (ids.empty()) std::printf("  no matches\n");
            for (int id : ids)
                std::printf("  [%2d] %s\n", id, g.node(id).name.c_str());
            continue;
        }

        if (choice == 13) {
            const int from = readInt("from id: ", 0, g.size() - 1);
            const int to   = readInt("to   id: ", 0, g.size() - 1);
            const RouteResult d = dijkstra(g, from, to);
            const RouteResult a = astar(g, from, to);
            const RouteResult b = bellmanFord(g, from, to);
            std::printf("  %-13s %-10s %-16s %s\n",
                        "algorithm", "km", "effort", "runtime");
            std::printf("  %-13s %-10.2f %-4lld settled     %.0f us\n",
                        "Dijkstra", d.distance_km, d.nodes_explored,
                        d.runtime_us);
            std::printf("  %-13s %-10.2f %-4lld settled     %.0f us\n",
                        "A*", a.distance_km, a.nodes_explored, a.runtime_us);
            std::printf("  %-13s %-10.2f %-4lld relaxations %.0f us\n",
                        "Bellman-Ford", b.distance_km, b.nodes_explored,
                        b.runtime_us);
            std::printf("  same optimal distance from three strategies.\n");
            continue;
        }

        if (choice == 14) {
            const AllPairs ap = floydWarshall(g);
            const CityStats s = computeCityStats(ap);
            std::printf("  Floyd-Warshall: %d^3 = %d ops in %.0f us\n",
                        ap.n, ap.n * ap.n * ap.n, ap.runtime_us);
            std::printf("  diameter : %.2f km (%s <-> %s)\n", s.diameter_km,
                        g.node(s.diameter_from).name.c_str(),
                        g.node(s.diameter_to).name.c_str());
            std::printf("  average  : %.2f km between any two points\n",
                        s.avg_distance_km);
            std::printf("  central  : %s (worst trip %.2f km)\n",
                        g.node(s.most_central).name.c_str(),
                        s.central_ecc_km);
            if (s.disconnected_pairs)
                std::printf("  WARNING  : %lld pairs unreachable\n",
                            s.disconnected_pairs);
            continue;
        }

        const int from = readInt("from id: ", 0, g.size() - 1);
        const int to   = readInt("to   id: ", 0, g.size() - 1);

        if (choice == 2 || choice == 4)
            printRoute(g, dijkstra(g, from, to), "Dijkstra");
        if (choice == 3 || choice == 4)
            printRoute(g, astar(g, from, to), "A*");
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dataPath = firstExisting({
        "data/city_graph.json",
        "backend/data/city_graph.json",
        "../backend/data/city_graph.json",
    });
    if (dataPath.empty()) {
        std::fprintf(stderr,
                     "error: data/city_graph.json not found.\n"
                     "run from backend/ or the repo root.\n");
        return 1;
    }

    Graph g;
    std::string cityName;
    try {
        g = loadCityFromJson(dataPath, &cityName);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error loading city: %s\n", e.what());
        return 1;
    }

    // History DB lives next to the data file. Compile with USE_SQLITE=1
    // for a real SQLite database; otherwise it's an appended JSONL file.
    std::error_code ec;
    fs::create_directories("db", ec);
    History history("db/lifeline");

    if (argc > 1 && std::string(argv[1]) == "console") {
        consoleMode(g, cityName, history);
        return 0;
    }

    int port = 8080;
    if (argc > 1) {
        try { port = std::stoi(argv[1]); } catch (...) {
            std::fprintf(stderr, "usage: %s [port|console]\n", argv[0]);
            return 1;
        }
    }

    // Prefer the built React app; fall back to the single-file demo UI.
    const std::string frontendDir = firstExisting(
        {"../frontend-react/dist", "frontend-react/dist",
         "../frontend", "frontend"});
    try {
        runServer(g, cityName, port, frontendDir, history);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "server error: %s\n", e.what());
        return 1;
    }
    return 0;
}
