# LifeLine — API Specification (v1, Phase 1)

**This file is the contract between backend (C++) and frontend (React).**
Rule for the team: change this file FIRST, get a 👍 on the group, then
change code. Frontend should never guess a response shape — it is written
here.

Base URL (dev): `http://localhost:8080`
All responses: `Content-Type: application/json`, CORS `*` enabled.

---

## GET /api/health

Liveness + quick stats. Frontend calls this on page load for the status pill.

```json
{ "status": "ok", "city": "Indrapur", "nodes": 28, "edges": 48, "phase": 1 }
```

---

## GET /api/city

Full graph for map rendering. Called once on page load.

```json
{
  "city": "Indrapur",
  "nodes": [
    { "id": 0, "name": "City Hospital", "type": "hospital",
      "lat": 19.088, "lon": 72.858 }
  ],
  "edges": [
    { "from": 7, "to": 12, "length_km": 1.02, "road": "Station Road",
      "capacity": 4000, "blocked": false }
  ]
}
```

Node `type` values currently used: `hospital, fire_station, police,
shelter, transit, market, office, education, residential, industrial,
infrastructure, junction, bridge`.

---

## GET /api/route?from={id}&to={id}&algo={dijkstra|astar}

Shortest path between two node ids. `algo` defaults to `dijkstra`.

Success:
```json
{
  "algo": "astar",
  "found": true,
  "path": [15, 26, 8, 23, 1],
  "path_names": ["Riverside Colony", "River Bridge South",
                 "Bus Terminal", "Airport Rd Junction", "Sunrise Hospital"],
  "coords": [[19.068, 72.871], [19.066, 72.877], [19.070, 72.884],
             [19.086, 72.884], [19.089, 72.894]],
  "distance_km": 4.78,
  "eta_min": 9.56,
  "nodes_explored": 8,
  "runtime_us": 42.0
}
```

`coords` is `[lat, lon]` pairs ready for a Leaflet polyline.
`nodes_explored` = settled nodes; use it for the Dijkstra-vs-A* compare UI.
If `found` is `false`, only `algo / found / nodes_explored / runtime_us`
are present.

Errors (HTTP 400): `{ "error": "human readable message" }`
- non-integer `from`/`to`
- id out of range
- unknown `algo`

---

## POST /api/disaster   *(Phase 2)*

Trigger a disaster. BFS spreads from `epicenter` for `severity` hops and
blocks every road inside the zone. Replaces any existing disaster.

Request body:
```json
{ "epicenter": 26, "type": "flood", "severity": 2 }
```
`type` is one of flood/fire/earthquake (cosmetic for now). `severity`
1–3 (clamped). Response:
```json
{
  "active": true,
  "epicenter": 26,
  "epicenter_name": "River Bridge South",
  "type": "flood",
  "severity": 2,
  "waves": [[26], [8, 11, 15, 21, 25], [5, 18, 20, ...]],
  "affected": [26, 8, 11, 15, 21, 25, ...],
  "blocked_edges": [{ "from": 8, "to": 11, "road": "Stadium Avenue" }],
  "blocked_count": 17
}
```
`waves[0]` is always the epicenter; each later array is one BFS ring —
render them as expanding circles.

## GET /api/disaster   *(Phase 2)*

Current disaster state (for page reloads). `{ "active": false }` if none.

## GET /api/evacuation   *(Phase 2)*

Max-flow from the danger zone to all safe shelters (shelters outside the
zone). Requires an active disaster (else HTTP 400).
```json
{
  "active": true,
  "sources": [26, 8, 15, ...],
  "sinks": [9, 10],
  "max_flow_per_hour": 15300,
  "bottlenecks": [{ "from": 0, "to": 9, "road": "Care Avenue",
                    "capacity": 2500 }],
  "augmenting_paths": 7,
  "runtime_us": 88.0
}
```
`bottlenecks` = the min cut. Their capacities sum to `max_flow_per_hour`
(max-flow/min-cut theorem). If every shelter is inside the zone, `sinks`
is empty, flow is 0, and a `note` field explains it.

## POST /api/reset   *(Phase 2)*

Clears the disaster and reopens all roads. `{ "ok": true }`.

---

## POST /api/dispatch   *(Phase 3)*

Triage (custom max-heap, severity 5→1, FIFO ties) + greedy assignment
(nearest free compatible vehicle by road distance via one
`dijkstraAll` per incident). Blocked roads are respected.

Request body — both keys optional:
```json
{
  "incidents": [{ "id": 1, "node": 17, "kind": "medical", "severity": 5 }],
  "vehicles":  [{ "id": 100, "node": 0, "type": "ambulance" }]
}
```
`kind`: `medical | fire | police | any`. `severity`: 1–5.
Omitted `vehicles` → default fleet parked at every hospital /
fire_station / police node. Omitted `incidents` → requires an active
disaster; incidents are generated across the zone with severity tied to
the BFS wave (epicenter = 5, ring 1 = 3, ring 2 = 1).

Response:
```json
{
  "assignments": [{
    "incident_id": 2, "severity": 5, "incident_kind": "fire",
    "incident_node": 19, "incident_name": "Industrial Area",
    "vehicle_id": 103, "vehicle_type": "fire_truck",
    "vehicle_node": 19, "vehicle_name": "Industrial Area",
    "distance_km": 0.0, "eta_min": 0.0,
    "coords": [[19.052, 72.898]], "path_names": ["Industrial Area"]
  }],
  "unassigned": [3],
  "assignments_by_type": { "fire_truck": 1 },
  "total_response_km": 0.0,
  "runtime_us": 310.0
}
```
`assignments` is in service order (most severe first). `unassigned` =
incident ids no compatible free vehicle can reach (e.g. roads cut off).
`coords` is the vehicle's drive path, ready for a Leaflet polyline.

## POST /api/supply   *(Phase 3)*

0/1 knapsack (DP with backtracking) for relief-truck loading.
```json
{ "capacity": 50,
  "items": [{ "name": "Water (20L)", "weight": 20, "value": 8 }] }
```
Empty body → default 8-item relief catalogue + 50 kg truck. Response:
`chosen` (the optimal subset), `total_weight`, `total_value`,
`capacity`, `runtime_us`.

## GET /api/history?limit=20   *(Phase 3)*

Recent activity, newest first.
```json
{ "backend": "file",
  "entries": [{ "timestamp": "2026-07-04 11:15:02", "event": "dispatch",
                "count": 3, "metric": 11.6,
                "detail": "3 assigned, 0 unassigned" }] }
```
`backend` is `"sqlite"` when the server was built with
`make USE_SQLITE=1`, otherwise `"file"` (JSONL).

---

## GET /api/resilience   *(Phase 4)*

Tarjan single-DFS analysis of the CURRENT road network (blocked roads
excluded): bridges, articulation points, connected components.
```json
{
  "components": 2,
  "bridges": [{ "from": 23, "to": 27, "road": "North Bridge Road",
                "coords": [[19.086,72.884],[19.09,72.877]] }],
  "articulation_points": [{ "id": 27, "name": "River Bridge North",
                            "lat": 19.09, "lon": 72.877 }],
  "isolated": [{ "id": 26, "name": "River Bridge South" }],
  "cut_off_from_hospital": [{ "id": 26, "name": "River Bridge South" }],
  "disaster_active": true
}
```
On the healthy city all four lists are empty and `components` is 1 —
the mesh has no single point of failure by design. Disasters create
them; that before/after contrast is the demo.

## GET /api/mst?algo=prim|kruskal|both   *(Phase 4)*

Minimum spanning tree — the restoration/backbone plan. Planning view:
ignores blocked flags. Default `both` returns each algorithm's edges,
`total_km`, `edge_count`, `runtime_us`, plus `totals_match` (must be
true: same optimum from two different algorithms).

## GET /api/suggest?q=riv&limit=8   *(Phase 4)*

Trie prefix autocomplete over location names (case-insensitive,
PREFIX not substring).
```json
{ "query": "riv",
  "results": [{ "id": 27, "name": "River Bridge North",
                "type": "bridge", "lat": 19.09, "lon": 72.877 }] }
```

---

## GET /api/compare?from=ID&to=ID   *(Phase 5 — Algorithm Lab)*

Runs Dijkstra, A*, and Bellman-Ford on the SAME query and reports the
work each did. All three must return the same optimal distance.
```json
{
  "from": 15, "from_name": "Riverside Colony",
  "to": 1,    "to_name": "Sunrise Hospital",
  "dijkstra":     { "found": true, "distance_km": 4.775,
                    "effort": 24, "effort_name": "nodes settled",
                    "runtime_us": 4.2 },
  "astar":        { "...": "effort 8 — heuristic-guided" },
  "bellman_ford": { "...": "effort = successful relaxations" },
  "all_distances_equal": true
}
```
`all_distances_equal` is the live correctness proof: three different
strategies, one optimum. Respects blocked roads.

## GET /api/citystats   *(Phase 5 — Floyd-Warshall analytics)*

Full all-pairs matrix (O(V³) = 21,952 ops on 28 nodes) distilled into
city-wide numbers. Recomputed on the CURRENT road state, so a disaster
visibly degrades them.
```json
{
  "nodes": 28,
  "fw_operations": 21952, "fw_runtime_us": 23.0,
  "diameter_km": 8.114,
  "diameter_from": "Hill View Colony", "diameter_to": "City Mall",
  "avg_distance_km": 3.993,
  "most_central": "Bus Terminal", "central_ecc_km": 4.643,
  "least_central": "...", "remote_ecc_km": 0.0,
  "disconnected_pairs": 0
}
```
During a severity-2 flood the average trip length jumps (~3.99 → ~5.59
km) and `disconnected_pairs` goes from 0 to 147 — one number that
summarises how badly the network is hurt.

---

Every algorithm in the synopsis is now implemented, endpoint-exposed,
and unit-tested: this spec is complete.
