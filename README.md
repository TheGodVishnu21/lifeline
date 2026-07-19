# LifeLine — Smart City Disaster Response & Evacuation Simulator

A DSA-driven full-stack project: a fictional city (**Indrapur**, 28 locations,
48 roads) modelled as a weighted graph, with a C++ engine that routes
emergency vehicles, simulates disasters, computes evacuation capacity with
max-flow, dispatches rescue units, and analyses network resilience — all on
data structures and algorithms written from scratch.

**🌐 Live demo:** https://lifeline-31iq.onrender.com — free-tier Render
instance, first load may take ~1 minute to cold-start.
**📄 Project report:** [docs/report_final.pdf](docs/report_final.pdf)

## Highlights

- **Custom data structures, no STL shortcuts** — binary min-heap & max-heap,
  separate-chaining hash map (djb2), trie, and Union-Find, all hand-written
  in `backend/src/ds/`.
- **10+ classic algorithms in production paths** — Dijkstra, A*,
  Bellman-Ford, Floyd-Warshall, Edmonds-Karp max-flow + min-cut, BFS disaster
  spread, Tarjan bridges/articulation points, Prim & Kruskal MST,
  0/1 knapsack DP, greedy dispatch.
- **Zero-dependency C++17 backend** — single static binary that serves both
  the REST API and the built React frontend (cpp-httplib, header-only).
- **React (Vite) command center** — interactive dark map with live disaster
  zones, reroutes, dispatch paths, and network analytics.
- **96 automated checks across 5 test suites**, cross-verified against
  `networkx` on the real city graph.

## Repo layout

```text
lifeline/
├── backend/
│   ├── include/            httplib.h, json.hpp (third-party, header-only)
│   ├── src/
│   │   ├── core/           graph.h/.cpp, city_loader (frozen interface)
│   │   ├── ds/             min_heap, max_heap, hash_map, trie (all custom)
│   │   ├── routing/        dijkstra (+dijkstraAll), astar,
│   │   │                   bellman_ford, floyd_warshall
│   │   ├── evacuation/     Edmonds-Karp max flow + min cut
│   │   ├── dispatch/       triage + greedy dispatch, knapsack supplies
│   │   ├── resilience/     UnionFind, Tarjan, Prim + Kruskal
│   │   ├── simulation/     BFS disaster spread
│   │   ├── db/             history (SQLite optional, JSONL fallback)
│   │   ├── api/            REST server (cpp-httplib)
│   │   └── main.cpp        server mode + console mode
│   ├── data/city_graph.json
│   ├── tests/              5 suites, 96 checks, networkx-verified
│   └── Makefile            `make`, `make USE_SQLITE=1`, `make test`
├── frontend-react/         React (Vite) command center
│   └── dist/               prebuilt — served by ./lifeline at /
├── frontend/map.html       zero-build fallback UI
├── docs/api_spec.md        backend↔frontend API contract
└── tools/generate_city.py  regenerates city_graph.json (keeps A* admissible)
```

## Build & run — Linux / macOS / WSL

```bash
git clone https://github.com/TheGodVishnu21/lifeline.git
cd lifeline/backend
make            # builds ./lifeline (zero external dependencies)
./lifeline      # REST server on http://localhost:8080
```

Open **http://localhost:8080** — the demo map loads automatically.

- Optional SQLite history: `make USE_SQLITE=1` (needs `libsqlite3-dev`);
  the default build logs to a JSONL file instead.
- Console mode (no browser needed): `./lifeline console`
- Custom port: `./lifeline 9090`
- Unit tests: `make test`

## Build & run — Windows (native, MSYS2)

1. Install MSYS2 from https://www.msys2.org (one-time).
2. Open the **"MSYS2 UCRT64"** shell and install the toolchain:
   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc make git
   ```
3. Clone, build, run (the Makefile auto-adds `-lws2_32` on Windows):
   ```bash
   git clone https://github.com/TheGodVishnu21/lifeline.git
   cd lifeline/backend
   make
   ./lifeline.exe
   ```

Prefer WSL? `wsl --install`, then follow the Linux steps above.

## Frontend (React)

A built `dist/` ships in the repo and is served by the C++ binary itself, so
Node is **not** needed at runtime. To rebuild or develop:

```bash
cd frontend-react
npm install
npm run build        # -> dist/, which ./lifeline auto-serves at /
npm run dev          # hot reload: Vite on :5173, /api proxied to :8080
```

`frontend/map.html` remains as a zero-build fallback UI.

---

## Feature tour

### Routing (Dijkstra vs A*)

Route **Riverside Colony → Sunrise Hospital** with algorithm **Compare**:
both find the same 4.78 km optimal path, but Dijkstra settles **24 nodes**
vs A*'s **8** — the haversine heuristic focuses the search toward the goal.
Hover any road for its name and length.

### Disaster + evacuation

1. Set spread radius to **1 ring**, epicenter **River Bridge South**, click
   **Trigger disaster**. Roads inside the zone turn red/dashed with
   expanding wave rings.
2. The same route now goes 5.42 km over the *North* bridge instead of the
   4.78 km south route — the engine reroutes around blocked roads
   automatically.
3. The **Evacuation Capacity** panel shows the max people/hour the road
   network can push to safe shelters, plus the exact **min-cut bottleneck
   roads**.
4. Bump radius to **2 rings**: the flood cuts both southern approaches and
   the west→east route becomes impossible — the map shows why.
5. **Clear** → everything reopens, the shortest route returns.

### Dispatch + supplies

1. With the flood active, click **🚑 Dispatch units**. Amber dashed lines
   show each vehicle's actual drive path; badges show severity (epicenter
   zone = highest).
2. Most severe incidents get vehicles **first** (triage max-heap), and each
   gets its **nearest** free compatible vehicle by road distance
   (greedy + Dijkstra).
3. Any ⚠ unreachable incidents are ones whose roads the flood cut — this
   ties directly back to the min-cut analysis.
4. **📦 Load truck**: 50 kg capacity; the knapsack DP picks the
   highest-relief-value subset (not the greedy value/weight order!).
5. The **Activity Log** is persisted — restart the server and it's still
   there (SQLite or JSONL).

### Resilience + search

1. Type `riv` in the search box — trie prefix search suggests all three
   River locations instantly; click one to fly there.
2. **🕸 Analyze network** on the healthy city: *0 bridges, 0 articulation
   points* — the city was deliberately designed with no single point of
   failure (minimum degree 3, 2-edge-connected mesh).
3. Trigger the flood and analyze again: **5 critical roads + 7 critical
   junctions** appear in violet, and River Bridge South is isolated with no
   hospital reachable. Same Tarjan DFS, different road state.
4. **🌿 Restoration plan**: the 28.07 km green backbone — with the proof
   line "Prim = Kruskal, two algorithms, one optimum."

---

## Design decisions (FAQ)

**Why is the A\* heuristic admissible?**
Every road length ≥ straight-line distance — enforced by
`tools/generate_city.py` — so h never overestimates. Admissible + consistent
⇒ A* explores fewer nodes yet returns the same optimal distance.

**Why no `std::priority_queue`?**
`src/ds/min_heap.h` is our own binary heap: sift-up/sift-down, O(log n)
push/pop, lazy deletion instead of decrease-key.

**What is a disaster's "spread"?**
Level-order BFS from the epicenter; each ring = one hop
(`src/simulation/disaster.cpp`).

**Why Edmonds-Karp, not plain Ford-Fulkerson?**
Augmenting paths are picked with **BFS** (shortest in edges), which bounds
the algorithm to O(V·E²) and avoids the pathological slow cases
(`src/evacuation/max_flow.cpp`).

**How are the bottleneck roads found?**
The **min cut** — a BFS over the residual graph after max flow. Their
capacities sum to the max flow; the unit tests assert this on the real city.

**Why a super-source/super-sink?**
Many danger nodes → many shelters is a multi-source multi-sink problem;
S and T with ∞-capacity arcs reduce it to single-pair max flow.

**Why a MAX-heap for triage, and how are ties broken?**
Custom binary max-heap in `src/ds/max_heap.h`; a monotonic sequence number
makes equal severities FIFO — stable triage.

**Is greedy assignment globally optimal?**
No — it's locally optimal per incident. The globally optimal assignment is
min-cost matching (Hungarian algorithm, O(n³)). Greedy was chosen because
dispatch is an *online* problem — incidents arrive over time — and greedy is
what real CAD systems approximate.

**Why ONE Dijkstra per incident instead of per vehicle?**
`dijkstraAll()` gives distances to ALL vehicles in a single run; the road
graph is undirected, so distance(vehicle→incident) =
distance(incident→vehicle).

**How does knapsack loading work?**
Classic 0/1 knapsack DP over integer weights; the chosen set is recovered by
backtracking the DP table (`src/dispatch/supply.cpp`).

**Where is the custom hash map used?**
Fleet counting in the dispatch response — separate chaining, djb2 hash,
load-factor 0.75 doubling.

**Why is SQLite optional?**
Compile-time `#ifdef USE_SQLITE` switch with a JSONL fallback —
zero-dependency default build; identical History interface either way
(`src/db/history.cpp`).

**How does ONE DFS find both bridges and articulation points?**
disc/low arrays; bridge iff `low[child] > disc[u]`, articulation point iff
`low[child] >= disc[u]`, root rule = 2+ children
(`src/resilience/critical.cpp`).

**Union-Find complexity?**
Path halving + union by rank ⇒ amortised inverse-Ackermann, effectively O(1)
(`src/resilience/union_find.h`).

**Prim vs Kruskal — why run both?**
Prim O(E log V) suits dense graphs, Kruskal O(E log E) suits sparse; both
run and their totals are asserted equal — with distinct edge weights the MST
is unique. Kruskal's edge ordering is a **heapsort** through our own MinHeap,
not `std::sort`.

**Trie vs hash map for autocomplete?**
Prefix queries in O(L) — a hash map can't enumerate "everything starting
with riv" without a full scan.

---

## Testing

```bash
cd backend
make test
```

5 suites (`routing`, `evacuation`, `dispatch`, `resilience`, `analytics`),
**96 checks**, all cross-verified against `networkx` on the real Indrapur
graph — shortest paths on all 784 node pairs, max-flow/min-cut duality,
bridge/articulation sets, MST totals, graph diameter and centrality.

## Project phases

- [x] **P1 Routing** — graph core, Dijkstra, A*, REST, demo map
- [x] **P2 Evacuation** — disaster spread (BFS), live road blocking,
      Edmonds-Karp max flow, min-cut bottlenecks
- [x] **P3 Dispatch** — custom max-heap triage, one-Dijkstra-per-incident
      greedy assignment, 0/1 knapsack supply loading, persistent history
      (SQLite via `make USE_SQLITE=1`, zero-dependency JSONL otherwise)
- [x] **P4 Resilience** — Union-Find (path halving + rank), Tarjan
      bridges/articulation points (networkx-verified), Prim + Kruskal MST
      (heap-sorted, totals cross-checked), Trie autocomplete
- [x] **P5 Frontend + Integration** — React (Vite) command center served
      by the C++ backend, Bellman-Ford + Floyd-Warshall algorithm lab,
      city analytics

## License

[MIT](LICENSE)
