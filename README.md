# LifeLine — Smart City Disaster Response & Evacuation Simulator

A DSA-driven full-stack project: a fictional city (**Indrapur**, 28 locations,
48 roads) modelled as a weighted graph, with a C++ engine that routes
emergency vehicles, and (in later phases) simulates disasters, computes
evacuation capacity with max-flow, and analyses network resilience.

**Phase 1 (this code): Routing Engine** — Dijkstra + A* on a custom
min-heap, REST API, interactive dark-map demo UI.

---

## Repo layout

```
lifeline/
├── backend/
│   ├── include/            httplib.h, json.hpp (third-party, header-only)
│   ├── src/
│   │   ├── core/           graph.h/.cpp, city_loader   ← FROZEN interface
│   │   ├── ds/             min_heap, max_heap, hash_map, trie (all custom)
│   │   ├── routing/        dijkstra (+dijkstraAll), astar      ← Member 1
│   │   ├── evacuation/     Edmonds-Karp max flow + min cut     ← Member 2
│   │   ├── dispatch/       triage+greedy dispatch, knapsack    ← Member 3
│   │   ├── resilience/     UnionFind, Tarjan, Prim+Kruskal  ← Member 4
│   │   ├── simulation/     BFS disaster spread
│   │   ├── db/             history (SQLite optional, JSONL fallback)
│   │   ├── api/            REST server (cpp-httplib)
│   │   └── main.cpp        server mode + console mode
│   ├── data/city_graph.json
│   ├── tests/              4 suites, 82 checks, networkx-verified
│   └── Makefile            `make`, `make USE_SQLITE=1`, `make test`
├── frontend-react/         React (Vite) command center  ← Member 5
│   └── dist/               prebuilt — served by ./lifeline at /
├── frontend/map.html       zero-build fallback UI
├── docs/api_spec.md        backend↔frontend contract — change this FIRST
├── docs/viva_pack.md       complexity table, 20 Q&A, demo script
└── tools/generate_city.py  regenerates city_graph.json (keeps A* admissible)
```

---

## Build & run — Linux / macOS / WSL

```bash
cd backend
make            # builds ./lifeline (zero external dependencies)
./lifeline      # REST server on http://localhost:8080
```

Optional: `make USE_SQLITE=1` stores history in a real SQLite DB
(needs `libsqlite3-dev`); the default build uses a JSONL file instead.

## Frontend (React)

```bash
cd frontend-react
npm install
npm run build        # -> dist/, which ./lifeline auto-serves at /
```

Restart `./lifeline` and open http://localhost:8080 — the React app is
served by the C++ backend itself (no Node needed at runtime; a built
`dist/` ships in the repo). For hot-reload development:
`npm run dev` (Vite on :5173, /api proxied to :8080).
`frontend/map.html` remains as a zero-build fallback UI.

Open **http://localhost:8080** → the demo map loads automatically.

Console mode (no browser needed): `./lifeline console`
Custom port: `./lifeline 9090`
Unit tests: `make test`

## Build & run — Windows (native, MSYS2)

1. Install MSYS2 from https://www.msys2.org (one-time).
2. Open the **"MSYS2 UCRT64"** shell and install the toolchain:
   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc make unzip
   ```
3. Build and run (the Makefile auto-adds `-lws2_32` on Windows):
   ```bash
   cd /c/Users/<you>/Downloads/lifeline/backend
   make
   ./lifeline.exe
   ```

Prefer WSL? `wsl --install`, then follow the Linux steps above — often
the smoothest path on Windows.

---

## 60-second demo script (for viva / class demo)

1. `./lifeline` → open http://localhost:8080
2. Route **Riverside Colony → Sunrise Hospital**, algorithm **Compare**.
3. Point at the result: *same 4.78 km optimal path*, but Dijkstra settled
   **24 nodes** vs A* only **8** — the haversine heuristic focuses the
   search toward the goal.
4. Hover any road → name + length. Hover any marker → location.
5. `./lifeline console` → same engine, zero UI dependencies.

Likely viva questions this phase already answers:
- Why is the heuristic admissible? (Every road length ≥ straight-line
  distance — enforced by `tools/generate_city.py` — so h never
  overestimates.)
- Why no `std::priority_queue`? (`src/ds/min_heap.h` is our own binary
  heap: sift-up/sift-down, O(log n) push/pop, lazy deletion instead of
  decrease-key.)
- Why does A* explore fewer nodes but return the same distance?
  (Admissible + consistent heuristic ⇒ optimality preserved.)

---

## 90-second demo script — Phase 2 (disaster + evacuation)

1. Set spread radius to **1 ring**, epicenter **River Bridge South**,
   click **Trigger disaster**. Roads inside the zone turn red/dashed,
   expanding wave rings appear.
2. Route **Riverside Colony → Sunrise Hospital**: it now goes 5.42 km over
   the *North* bridge instead of the 4.78 km south route — the engine
   rerouted around blocked roads automatically.
3. Read the **Evacuation Capacity** panel: max people/hour the road
   network can push to safe shelters, plus the exact **min-cut bottleneck
   roads**.
4. Bump radius to **2 rings**: the flood cuts both southern approaches and
   the west→east route becomes impossible — the map shows why.
5. **Clear** → everything reopens, shortest route returns.

Phase-2 viva questions this answers:
- What is a disaster's "spread"? (Level-order BFS from the epicenter; each
  ring = one hop. `src/simulation/disaster.cpp`.)
- Why Edmonds-Karp, not plain Ford-Fulkerson? (We pick augmenting paths
  with **BFS** — shortest path in edges — which bounds it to O(V·E²) and
  avoids the pathological slow cases. `src/evacuation/max_flow.cpp`.)
- What are the bottleneck roads? (The **min cut** — found by BFS over the
  residual graph after max flow. Their capacities sum to the max flow;
  the unit tests assert this on the real city.)
- Why super-source/super-sink? (Many danger nodes → many shelters is a
  multi-source multi-sink problem; S and T with ∞-capacity arcs reduce it
  to a single-pair max flow.)

## 60-second demo script — Phase 3 (dispatch + supplies)

1. Trigger a **1-ring flood at River Bridge South**, then click
   **🚑 Dispatch units**. Amber dashed lines = each vehicle's actual
   drive path; badges show severity (epicenter zone = highest).
2. Point out the order: most severe incidents got vehicles FIRST
   (triage max-heap), and each got its **nearest** free compatible
   vehicle by road distance (greedy + Dijkstra).
3. Note any ⚠ unreachable incidents — the flood cut their roads; this
   ties directly back to the min-cut analysis.
4. Click **📦 Load truck**: 50 kg capacity, the knapsack DP picks the
   highest-relief-value subset (not the greedy value/weight order!).
5. The **Activity Log** at the bottom is persisted — restart the server
   and it's still there (SQLite or JSONL file).

Phase-3 viva questions this answers:
- Why a MAX-heap for triage, and how are ties broken? (Custom binary
  max-heap in `src/ds/max_heap.h`; a monotonic sequence number makes
  equal severities FIFO — stable triage.)
- Is greedy assignment globally optimal? (**No** — it's locally optimal
  per incident. Globally optimal assignment is min-cost matching /
  Hungarian algorithm, O(n³). We chose greedy because dispatch is an
  *online* problem — incidents arrive over time — and greedy is what
  real CAD systems approximate. Great discussion answer.)
- Why ONE Dijkstra per incident instead of per vehicle?
  (`dijkstraAll()` gives distances to ALL vehicles in a single run;
  the road graph is undirected so distance(vehicle→incident) =
  distance(incident→vehicle).)
- Knapsack: recurrence, why integer weights, how the chosen set is
  recovered (backtracking the DP table — `src/dispatch/supply.cpp`).
- Where is the custom hash map used? (Fleet counting in the dispatch
  response — separate chaining, djb2 hash, load-factor 0.75 doubling.)
- Why is SQLite optional? (Compile-time `#ifdef USE_SQLITE` switch with
  a JSONL fallback — zero-dependency default build; identical History
  interface either way. `src/db/history.cpp`.)

## 60-second demo script — Phase 4 (resilience + search)

1. Type `riv` in the search box — Trie prefix search suggests all three
   River locations instantly; click one to fly there.
2. Click **🕸 Analyze network** on the healthy city: *0 bridges,
   0 articulation points* — "hamari city me single point of failure hai
   hi nahi, min degree 3 rakha hai."
3. Trigger the flood at River Bridge South, analyze again: suddenly
   **5 critical roads + 7 critical junctions** appear in violet, and
   River Bridge South is isolated with no hospital reachable. Same
   Tarjan DFS, different road state.
4. Click **🌿 Restoration plan**: the 28.07 km green backbone — and the
   proof line "Prim = Kruskal, two algorithms, one optimum."

Phase-4 viva questions this answers:
- How does ONE DFS find both bridges and articulation points?
  (disc/low arrays; bridge iff `low[child] > disc[u]`, AP iff
  `low[child] >= disc[u]`, root rule = 2+ children.
  `src/resilience/critical.cpp`.)
- Union-Find complexity? (Path halving + union by rank ⇒ amortised
  inverse-Ackermann, effectively O(1). `src/resilience/union_find.h`.)
- Prim vs Kruskal — when to prefer which? (Prim O(E log V) suits dense
  graphs, Kruskal O(E log E) suits sparse; we run BOTH and assert equal
  totals — with distinct edge weights the MST is unique.)
- Where is sorting? (Kruskal's edge ordering is a HEAPSORT through our
  own MinHeap — no `std::sort`.)
- Trie vs hashmap for autocomplete? (Prefix queries in O(L) — a hashmap
  can't enumerate "everything starting with riv" without a full scan.)
- Why does the healthy city report nothing critical? (Every node has
  degree ≥ 3 and the mesh is 2-edge-connected — deliberate data design;
  disasters are what create critical structure.)

## Team map (who reads what)

| Member | Module | Start with |
|--------|--------------------------|-----------------------------------|
| 1 | Routing | `src/routing/`, `src/ds/min_heap.h` |
| 2 | Evacuation (max flow) | `src/core/graph.h` (`capacity` field is for you) |
| 3 | Dispatch (heaps/DP) | `src/dispatch/`, `src/ds/max_heap.h`, `src/ds/hash_map.h` |
| 4 | Resilience (UF/MST/bridges) | `src/resilience/`, `src/ds/trie.h` |
| 5 | Frontend | `frontend-react/src/` + `docs/api_spec.md` |

Git rule: one branch per module (`routing`, `evacuation`, `dispatch`,
`resilience`, `frontend`), PRs into `main`, no direct pushes.

---

## Roadmap

- [x] **P1 Routing** — graph core, Dijkstra, A*, REST, demo map
- [x] **P2 Evacuation** — disaster spread (BFS), live road blocking,
      Ford-Fulkerson/Edmonds-Karp max flow, min-cut bottlenecks
- [x] **P3 Dispatch** — custom max-heap triage, one-Dijkstra-per-incident
      greedy assignment, 0/1 knapsack supply loading, history log
      (SQLite via `make USE_SQLITE=1`, zero-dependency JSONL otherwise)
- [x] **P4 Resilience** — Union-Find (path halving + rank), Tarjan
      bridges/articulation points (networkx-verified), Prim + Kruskal MST
      (heap-sorted, totals cross-checked), Trie autocomplete
- [x] **P5 Frontend + Integration** — React (Vite) command center served
      by the C++ backend, Bellman-Ford + Floyd-Warshall algorithm lab,
      city analytics, docs/viva_pack.md

**PROJECT COMPLETE** — every synopsis concept implemented, used, and
tested (96 checks + networkx cross-verification).
