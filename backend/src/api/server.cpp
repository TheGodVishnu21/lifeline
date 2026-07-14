#include "api/server.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#include "httplib.h"
#include "json.hpp"

#include "db/history.h"
#include "dispatch/dispatch.h"
#include "dispatch/supply.h"
#include "ds/hash_map.h"
#include "ds/trie.h"
#include "evacuation/max_flow.h"
#include "resilience/critical.h"
#include "resilience/mst.h"
#include "routing/astar.h"
#include "routing/bellman_ford.h"
#include "routing/dijkstra.h"
#include "routing/floyd_warshall.h"
#include "simulation/disaster.h"

namespace lifeline {

using nlohmann::json;

namespace {

json routeToJson(const Graph& g, const RouteResult& r,
                 const std::string& algo) {
    json out;
    out["algo"] = algo;
    out["found"] = r.found;
    out["nodes_explored"] = r.nodes_explored;
    out["runtime_us"] = r.runtime_us;
    if (!r.found) return out;

    json path = json::array();
    json names = json::array();
    json coords = json::array();
    for (int id : r.path) {
        const Node& n = g.node(id);
        path.push_back(id);
        names.push_back(n.name);
        coords.push_back({n.lat, n.lon});
    }
    out["path"] = path;
    out["path_names"] = names;
    out["coords"] = coords;                       // ready for Leaflet polyline
    out["distance_km"] = r.distance_km;
    out["eta_min"] = r.distance_km / 30.0 * 60.0; // assume 30 km/h city speed
    return out;
}

void sendJson(httplib::Response& res, const json& j, int status = 200) {
    res.status = status;
    res.set_content(j.dump(2), "application/json");
}

void sendError(httplib::Response& res, int status, const std::string& msg) {
    sendJson(res, json{{"error", msg}}, status);
}

std::string roadName(const Graph& g, int u, int v) {
    for (const Edge& e : g.neighbors(u))
        if (e.to == v) return e.road;
    return "";
}

json disasterToJson(const Graph& g, const Disaster& d) {
    json out;
    out["active"] = true;
    out["epicenter"] = d.epicenter;
    out["epicenter_name"] = g.node(d.epicenter).name;
    out["type"] = d.type;
    out["severity"] = d.severity;
    out["waves"] = d.waves;
    out["affected"] = d.affected;

    json blocked = json::array();
    for (auto [u, v] : d.blocked)
        blocked.push_back({{"from", u}, {"to", v},
                           {"road", roadName(g, u, v)}});
    out["blocked_edges"] = blocked;
    out["blocked_count"] = d.blocked.size();
    return out;
}

json assignmentToJson(const Graph& g, const Assignment& a) {
    json coords = json::array();
    json names = json::array();
    for (int id : a.path) {
        const Node& n = g.node(id);
        coords.push_back({n.lat, n.lon});
        names.push_back(n.name);
    }
    return json{{"incident_id", a.incident_id},
                {"severity", a.severity},
                {"incident_kind", a.incident_kind},
                {"incident_node", a.incident_node},
                {"incident_name", g.node(a.incident_node).name},
                {"vehicle_id", a.vehicle_id},
                {"vehicle_type", a.vehicle_type},
                {"vehicle_node", a.vehicle_node},
                {"vehicle_name", g.node(a.vehicle_node).name},
                {"distance_km", a.distance_km},
                {"eta_min", a.eta_min},
                {"coords", coords},
                {"path_names", names}};
}

}  // namespace

void runServer(Graph& g, const std::string& cityName, int port,
               const std::string& frontendDir, History& history) {
    httplib::Server svr;

    // httplib serves requests on multiple threads; the disaster endpoints
    // mutate the graph (blocked flags), so every handler that touches g
    // or the active disaster takes this lock.
    std::mutex mtx;
    std::optional<Disaster> active;

    // Location autocomplete: built ONCE at startup. Names never change,
    // so the suggest handler reads it without taking the lock —
    // immutable data needs no synchronisation.
    Trie trie;
    for (const Node& n : g.nodes()) trie.insert(n.name, n.id);

    // CORS: allow the demo page to fetch from file:// or another port.
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}});
    svr.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });

    // ---- GET /api/health -------------------------------------------
    svr.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        sendJson(res, json{{"status", "ok"},
                           {"city", cityName},
                           {"nodes", g.size()},
                           {"edges", g.edgeCount()},
                           {"disaster_active", active.has_value()},
                           {"history_backend", history.usingSqlite() ? "sqlite" : "file"},
                           {"phase", 5}});
    });

    // ---- GET /api/city  (full graph for map rendering) -------------
    svr.Get("/api/city", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        json nodes = json::array();
        for (const Node& n : g.nodes())
            nodes.push_back({{"id", n.id}, {"name", n.name},
                             {"type", n.type}, {"lat", n.lat},
                             {"lon", n.lon}});

        json edges = json::array();
        for (int u = 0; u < g.size(); ++u)
            for (const Edge& e : g.neighbors(u))
                if (u < e.to)  // each undirected road once
                    edges.push_back({{"from", u}, {"to", e.to},
                                     {"length_km", e.length_km},
                                     {"road", e.road},
                                     {"capacity", e.capacity},
                                     {"blocked", e.blocked}});

        sendJson(res, json{{"city", cityName},
                           {"nodes", nodes}, {"edges", edges}});
    });

    // ---- GET /api/route?from=ID&to=ID&algo=dijkstra|astar ----------
    svr.Get("/api/route", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        int from = -1, to = -1;
        try {
            from = std::stoi(req.get_param_value("from"));
            to = std::stoi(req.get_param_value("to"));
        } catch (...) {
            return sendError(res, 400,
                             "params 'from' and 'to' must be integer node ids");
        }
        if (!g.validId(from) || !g.validId(to))
            return sendError(res, 400, "node id out of range (0.." +
                                           std::to_string(g.size() - 1) + ")");

        std::string algo = req.get_param_value("algo");
        if (algo.empty()) algo = "dijkstra";
        if (algo != "dijkstra" && algo != "astar")
            return sendError(res, 400, "algo must be 'dijkstra' or 'astar'");

        const RouteResult r =
            (algo == "astar") ? astar(g, from, to) : dijkstra(g, from, to);
        sendJson(res, routeToJson(g, r, algo));
    });

    // ---- POST /api/disaster  {"epicenter":26,"type":"flood","severity":2}
    svr.Post("/api/disaster", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            return sendError(res, 400, "body must be JSON");
        }
        if (!body.contains("epicenter") || !body["epicenter"].is_number_integer())
            return sendError(res, 400, "'epicenter' (integer node id) is required");

        const int epicenter = body["epicenter"].get<int>();
        if (!g.validId(epicenter))
            return sendError(res, 400, "epicenter out of range (0.." +
                                           std::to_string(g.size() - 1) + ")");

        // one active disaster at a time: new one replaces the old
        clearDisaster(g);
        active = simulateDisaster(g, epicenter,
                                  body.value("type", "flood"),
                                  body.value("severity", 1));
        history.add({"", "disaster",
                     static_cast<int>(active->affected.size()),
                     static_cast<double>(active->blocked.size()),
                     active->type + " @ " + g.node(epicenter).name +
                         " sev" + std::to_string(active->severity)});
        sendJson(res, disasterToJson(g, *active));
    });

    // ---- GET /api/disaster  (current state, for page reloads) ------
    svr.Get("/api/disaster", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!active) return sendJson(res, json{{"active", false}});
        sendJson(res, disasterToJson(g, *active));
    });

    // ---- GET /api/evacuation  (max flow: danger zone -> safe shelters)
    svr.Get("/api/evacuation", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!active)
            return sendError(res, 400,
                             "no active disaster — POST /api/disaster first");

        std::vector<int> sinks;
        std::vector<bool> inZone(g.size(), false);
        for (int id : active->affected) inZone[id] = true;
        for (const Node& n : g.nodes())
            if (n.type == "shelter" && !inZone[n.id]) sinks.push_back(n.id);

        const EvacResult r = computeEvacuation(g, active->affected, sinks);

        json bn = json::array();
        for (const Bottleneck& b : r.bottlenecks)
            bn.push_back({{"from", b.from}, {"to", b.to},
                          {"road", b.road}, {"capacity", b.capacity}});

        json out{{"active", true},
                 {"epicenter", active->epicenter},
                 {"sources", r.sources},
                 {"sinks", r.sinks},
                 {"max_flow_per_hour", r.max_flow},
                 {"bottlenecks", bn},
                 {"augmenting_paths", r.augmenting_paths},
                 {"runtime_us", r.runtime_us}};
        if (r.sinks.empty())
            out["note"] = "every shelter is inside the disaster zone";
        sendJson(res, out);
    });

    // ---- POST /api/reset  ------------------------------------------
    svr.Post("/api/reset", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        clearDisaster(g);
        active.reset();
        sendJson(res, json{{"ok", true}});
    });

    // ---- POST /api/dispatch  (triage heap + greedy assignment) -----
    // Body: { "incidents":[{"id":1,"node":17,"kind":"medical","severity":5}],
    //         "vehicles":[{"id":100,"node":3,"type":"ambulance"}] }
    // If either list is omitted, a default fleet / the active disaster's
    // zone are used so the endpoint is demo-friendly with an empty body.
    svr.Post("/api/dispatch", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        json body = json::object();
        if (!req.body.empty()) {
            try { body = json::parse(req.body); }
            catch (...) { return sendError(res, 400, "body must be JSON"); }
        }

        std::vector<Incident> incidents;
        if (body.contains("incidents")) {
            int auto_id = 1;
            for (const auto& j : body["incidents"]) {
                Incident inc;
                inc.id = j.value("id", auto_id++);
                inc.node = j.value("node", -1);
                inc.kind = j.value("kind", "any");
                inc.severity = j.value("severity", 1);
                if (!g.validId(inc.node))
                    return sendError(res, 400,
                                     "incident node out of range");
                incidents.push_back(inc);
            }
        } else if (active) {
            // demo default: incidents across the disaster zone, severity
            // tied to the BFS wave — epicenter (wave 0) is critical (5),
            // the next ring is 3, the outer ring is 1. Closer to the
            // epicenter => more severe.
            int id = 1;
            for (size_t wi = 0; wi < active->waves.size(); ++wi) {
                const int sev = std::max(1, 5 - 2 * static_cast<int>(wi));
                for (int node : active->waves[wi]) {
                    if (g.node(node).type == "shelter") continue;
                    incidents.push_back({id++, node, "any", sev});
                }
            }
        } else {
            return sendError(res, 400,
                "provide 'incidents' or trigger a disaster first");
        }

        std::vector<Vehicle> vehicles;
        if (body.contains("vehicles")) {
            int auto_id = 100;
            for (const auto& j : body["vehicles"]) {
                Vehicle v;
                v.id = j.value("id", auto_id++);
                v.node = j.value("node", -1);
                v.type = j.value("type", "ambulance");
                v.available = j.value("available", true);
                if (!g.validId(v.node))
                    return sendError(res, 400, "vehicle node out of range");
                vehicles.push_back(v);
            }
        } else {
            // demo default fleet: park responders at their stations
            for (const Node& n : g.nodes()) {
                if (n.type == "hospital")
                    vehicles.push_back({(int)vehicles.size() + 100, n.id,
                                        "ambulance", true});
                else if (n.type == "fire_station")
                    vehicles.push_back({(int)vehicles.size() + 100, n.id,
                                        "fire_truck", true});
                else if (n.type == "police")
                    vehicles.push_back({(int)vehicles.size() + 100, n.id,
                                        "police", true});
            }
        }

        const DispatchResult r = dispatch(g, incidents, vehicles);

        json arr = json::array();
        for (const Assignment& a : r.assignments)
            arr.push_back(assignmentToJson(g, a));

        // Count assignments per vehicle type with OUR hash map (custom
        // separate-chaining table from ds/hash_map.h) — O(1) average per
        // update, and a live demo of the structure in real use.
        HashMap<int> byType;
        for (const Assignment& a : r.assignments)
            byType.put(a.vehicle_type, byType.getOr(a.vehicle_type, 0) + 1);
        json fleetJson = json::object();
        byType.forEach([&](const std::string& k, int v) { fleetJson[k] = v; });

        history.add({"", "dispatch",
                     static_cast<int>(r.assignments.size()),
                     r.total_response_km,
                     std::to_string(r.assignments.size()) + " assigned, " +
                         std::to_string(r.unassigned.size()) + " unassigned"});

        sendJson(res, json{{"assignments", arr},
                           {"unassigned", r.unassigned},
                           {"assignments_by_type", fleetJson},
                           {"total_response_km", r.total_response_km},
                           {"runtime_us", r.runtime_us}});
    });

    // ---- POST /api/supply  (0/1 knapsack truck loading) ------------
    // Body: { "capacity":50,
    //         "items":[{"name":"Water","weight":10,"value":8}, ...] }
    // Empty body => a default relief-item catalogue + 50 kg truck.
    svr.Post("/api/supply", [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::object();
        if (!req.body.empty()) {
            try { body = json::parse(req.body); }
            catch (...) { return sendError(res, 400, "body must be JSON"); }
        }

        std::vector<SupplyItem> items;
        if (body.contains("items")) {
            for (const auto& j : body["items"])
                items.push_back({j.value("name", "item"),
                                 j.value("weight", 0),
                                 j.value("value", 0)});
        } else {
            items = {{"Water (20L)", 20, 8}, {"First-aid kit", 5, 10},
                     {"Food rations", 15, 7}, {"Blankets", 8, 4},
                     {"Medicine box", 6, 9}, {"Tents", 25, 6},
                     {"Flashlights", 3, 3}, {"Power generator", 30, 5}};
        }
        const int capacity = body.value("capacity", 50);

        const SupplyPlan p = planSupplies(items, capacity);
        json chosen = json::array();
        for (const auto& it : p.chosen)
            chosen.push_back({{"name", it.name}, {"weight", it.weight},
                              {"value", it.value}});
        sendJson(res, json{{"capacity", p.capacity},
                           {"chosen", chosen},
                           {"total_weight", p.total_weight},
                           {"total_value", p.total_value},
                           {"runtime_us", p.runtime_us}});
    });

    // ---- GET /api/history  -----------------------------------------
    svr.Get("/api/history", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);  // history writes happen
        int limit = 20;                          // under the same lock
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); }
            catch (...) {}
        }
        json arr = json::array();
        for (const HistoryEntry& e : history.recent(limit))
            arr.push_back({{"timestamp", e.timestamp}, {"event", e.event},
                           {"count", e.count}, {"metric", e.metric},
                           {"detail", e.detail}});
        sendJson(res, json{{"backend", history.usingSqlite() ? "sqlite" : "file"},
                           {"entries", arr}});
    });

    // ---- GET /api/resilience  (Tarjan bridges + APs + components) ---
    svr.Get("/api/resilience", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);   // reads blocked flags
        const CriticalReport r = analyzeCritical(g);

        json bridges = json::array();
        for (auto [u, v] : r.bridges) {
            const Node& a = g.node(u);
            const Node& b = g.node(v);
            bridges.push_back({{"from", u}, {"to", v},
                               {"road", roadName(g, u, v)},
                               {"coords", {{a.lat, a.lon}, {b.lat, b.lon}}}});
        }

        json aps = json::array();
        for (int id : r.articulation_points)
            aps.push_back({{"id", id}, {"name", g.node(id).name},
                           {"lat", g.node(id).lat}, {"lon", g.node(id).lon}});

        // isolated zones = every component beyond the largest
        json isolated = json::array();
        for (size_t i = 1; i < r.component_members.size(); ++i)
            for (int id : r.component_members[i])
                isolated.push_back({{"id", id}, {"name", g.node(id).name}});

        // components that cannot reach ANY hospital
        json noHospital = json::array();
        for (const auto& comp : r.component_members) {
            bool hasHospital = false;
            for (int id : comp)
                if (g.node(id).type == "hospital") { hasHospital = true; break; }
            if (!hasHospital)
                for (int id : comp)
                    noHospital.push_back({{"id", id}, {"name", g.node(id).name}});
        }

        sendJson(res, json{{"components", r.components},
                           {"bridges", bridges},
                           {"articulation_points", aps},
                           {"isolated", isolated},
                           {"cut_off_from_hospital", noHospital},
                           {"disaster_active", active.has_value()}});
    });

    // ---- GET /api/mst?algo=prim|kruskal|both  (restoration plan) ----
    svr.Get("/api/mst", [&](const httplib::Request& req, httplib::Response& res) {
        // Planning view over the FULL network (ignores blocked flags),
        // and the graph topology is immutable — no lock needed.
        std::string algo = req.get_param_value("algo");
        if (algo.empty()) algo = "both";
        if (algo != "prim" && algo != "kruskal" && algo != "both")
            return sendError(res, 400, "algo must be prim, kruskal or both");

        auto toJson = [&](const MstResult& m) {
            json edges = json::array();
            for (const MstEdge& e : m.edges) {
                const Node& a = g.node(e.u);
                const Node& b = g.node(e.v);
                edges.push_back({{"from", e.u}, {"to", e.v},
                                 {"road", e.road},
                                 {"length_km", e.length_km},
                                 {"coords", {{a.lat, a.lon}, {b.lat, b.lon}}}});
            }
            return json{{"algo", m.algo}, {"total_km", m.total_km},
                        {"edge_count", m.edges.size()},
                        {"runtime_us", m.runtime_us}, {"edges", edges}};
        };

        json out;
        if (algo == "prim" || algo == "both") out["prim"] = toJson(primMst(g));
        if (algo == "kruskal" || algo == "both")
            out["kruskal"] = toJson(kruskalMst(g));
        if (algo == "both")
            out["totals_match"] =
                std::fabs(out["prim"]["total_km"].get<double>() -
                          out["kruskal"]["total_km"].get<double>()) < 1e-6;
        sendJson(res, out);
    });

    // ---- GET /api/suggest?q=riv&limit=8  (Trie autocomplete) --------
    svr.Get("/api/suggest", [&](const httplib::Request& req, httplib::Response& res) {
        // trie is immutable after startup — lock-free read is safe
        const std::string q = req.get_param_value("q");
        int limit = 8;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); }
            catch (...) {}
        }
        json results = json::array();
        if (!q.empty())
            for (int id : trie.suggest(q, limit))
                results.push_back({{"id", id}, {"name", g.node(id).name},
                                   {"type", g.node(id).type},
                                   {"lat", g.node(id).lat},
                                   {"lon", g.node(id).lon}});
        sendJson(res, json{{"query", q}, {"results", results}});
    });

    // ---- GET /api/compare?from=ID&to=ID  (algorithm lab) ------------
    // Dijkstra vs A* vs Bellman-Ford on the same pair. All three MUST
    // report the same distance — the differences are in HOW MUCH WORK
    // each did to find it.
    svr.Get("/api/compare", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        int from = -1, to = -1;
        try {
            from = std::stoi(req.get_param_value("from"));
            to = std::stoi(req.get_param_value("to"));
        } catch (...) {
            return sendError(res, 400, "integer 'from' and 'to' required");
        }
        if (!g.validId(from) || !g.validId(to))
            return sendError(res, 400, "node id out of range");

        const RouteResult d = dijkstra(g, from, to);
        const RouteResult a = astar(g, from, to);
        const RouteResult b = bellmanFord(g, from, to);

        auto one = [](const RouteResult& r, const char* effortName) {
            return json{{"found", r.found},
                        {"distance_km", r.distance_km},
                        {"effort", r.nodes_explored},
                        {"effort_name", effortName},
                        {"runtime_us", r.runtime_us}};
        };
        const bool allEqual =
            d.found == a.found && a.found == b.found &&
            (!d.found || (std::fabs(d.distance_km - a.distance_km) < 1e-6 &&
                          std::fabs(d.distance_km - b.distance_km) < 1e-6));

        sendJson(res, json{{"from", from}, {"to", to},
                           {"from_name", g.node(from).name},
                           {"to_name", g.node(to).name},
                           {"dijkstra", one(d, "nodes settled")},
                           {"astar", one(a, "nodes settled")},
                           {"bellman_ford", one(b, "relaxations")},
                           {"all_distances_equal", allEqual}});
    });

    // ---- GET /api/citystats  (Floyd-Warshall analytics) -------------
    svr.Get("/api/citystats", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);   // respects blocked roads
        const AllPairs ap = floydWarshall(g);
        const CityStats s = computeCityStats(ap);

        json out{{"nodes", ap.n},
                 {"fw_runtime_us", ap.runtime_us},
                 {"fw_operations", ap.n * ap.n * ap.n},
                 {"diameter_km", s.diameter_km},
                 {"diameter_from", g.node(s.diameter_from).name},
                 {"diameter_to", g.node(s.diameter_to).name},
                 {"avg_distance_km", s.avg_distance_km},
                 {"most_central", g.node(s.most_central).name},
                 {"central_ecc_km", s.central_ecc_km},
                 {"least_central", g.node(s.least_central).name},
                 {"remote_ecc_km", s.remote_ecc_km},
                 {"disconnected_pairs", s.disconnected_pairs},
                 {"disaster_active", active.has_value()}};
        sendJson(res, out);
    });

    // ---- static demo UI at / ---------------------------------------
    if (!frontendDir.empty()) {
        svr.set_mount_point("/", frontendDir);
        // make "/" serve map.html instead of a directory listing
        svr.Get("/", [frontendDir](const httplib::Request&, httplib::Response& res) {
            res.set_redirect("/map.html");
        });
    }

    std::printf("LifeLine backend  |  city: %s  |  %d nodes, %d edges\n",
                cityName.c_str(), g.size(), g.edgeCount());
    std::printf("REST API   -> http://localhost:%d/api/health\n", port);
    if (!frontendDir.empty())
        std::printf("Demo map   -> http://localhost:%d/\n", port);
    std::printf("Press Ctrl+C to stop.\n");

    if (!svr.listen("0.0.0.0", port))
        throw std::runtime_error("could not bind port " + std::to_string(port));
}

}  // namespace lifeline
