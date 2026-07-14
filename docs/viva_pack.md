# LifeLine — Viva Pack

Print this. Every member reads their section + the master table.

---

## 1. Master complexity table

| Algorithm / DS | File | Complexity | Used for |
|---|---|---|---|
| BFS (level-order) | `simulation/disaster.cpp` | O(V+E) | disaster spread rings |
| DFS (Tarjan) | `resilience/critical.cpp` | O(V+E) | bridges + articulation points |
| Dijkstra | `routing/dijkstra.cpp` | O((V+E) log V) | ambulance routing |
| `dijkstraAll` | `routing/dijkstra.cpp` | O((V+E) log V) | one run → distances to ALL vehicles |
| A* | `routing/astar.cpp` | O((V+E) log V), explores less | goal-directed routing |
| Bellman-Ford | `routing/bellman_ford.cpp` | O(V·E) | relaxation baseline, neg-weight safe |
| Floyd-Warshall | `routing/floyd_warshall.cpp` | O(V³) | all-pairs matrix, city stats |
| Edmonds-Karp max flow | `evacuation/max_flow.cpp` | O(V·E²) | evacuation people/hour |
| Min cut | `evacuation/max_flow.cpp` | O(V+E) after flow | bottleneck roads |
| Prim MST | `resilience/mst.cpp` | O(E log V) | restoration backbone |
| Kruskal MST | `resilience/mst.cpp` | O(E log E) | same — cross-check |
| Union-Find | `resilience/union_find.h` | ~O(α(n)) amortised | Kruskal + components |
| MinHeap (custom) | `ds/min_heap.h` | push/pop O(log n) | Dijkstra, A*, Prim, Kruskal sort |
| MaxHeap (custom) | `ds/max_heap.h` | push/pop O(log n) | incident triage (stable ties) |
| HashMap (custom) | `ds/hash_map.h` | O(1) avg | fleet counting |
| Trie (custom) | `ds/trie.h` | O(L) prefix | location autocomplete |
| 0/1 Knapsack DP | `dispatch/supply.cpp` | O(n·W) | relief truck loading |
| Greedy | `dispatch/dispatch.cpp` | per incident | nearest-vehicle assignment |
| Heapsort | inside Kruskal | O(E log E) | edge ordering, no std::sort |

Every custom DS is **used in the live product**, not just defined.

---

## 2. Twenty most likely viva questions

**Q1. Why is the A\* heuristic admissible?**
h = straight-line haversine km. Every road length = haversine × factor,
factor ≥ 1.05 (enforced by `tools/generate_city.py`), so h never
overestimates → A\* optimal. Tests prove A\* distance == Dijkstra on all
784 pairs.

**Q2. Why no `std::priority_queue` / `std::unordered_map` / `std::sort`?**
The point of the project is implementing them: `min_heap.h`,
`max_heap.h`, `hash_map.h`, `trie.h`, and Kruskal heapsorts through our
own heap.

**Q3. How does lazy deletion replace decrease-key?**
Push a duplicate with the better distance; when popping, skip anything
already settled. Same asymptotics, far simpler code.

**Q4. Why does A\* explore fewer nodes but give the same answer?**
Admissible + consistent heuristic keeps optimality; the h-term steers
the frontier toward the goal (24 → 8 settled on the demo route).

**Q5. Edmonds-Karp vs plain Ford-Fulkerson?**
Augmenting paths chosen by **BFS** (fewest edges) → O(V·E²) bound and
no pathological capacity-dependent behaviour.

**Q6. Prove your min cut is right.**
Max-flow/min-cut theorem: sum of cut capacities == max flow. The test
suite asserts that equality on the real city every run.

**Q7. Why super-source/super-sink?**
Many danger nodes → many shelters is multi-source/multi-sink; ∞-cap
arcs from S and to T reduce it to standard single-pair max flow.

**Q8. How does one DFS find bridges AND articulation points?**
disc/low arrays. Bridge iff `low[child] > disc[u]`; AP iff
`low[child] >= disc[u]` (root: 2+ DFS children).

**Q9. Why does healthy Indrapur have zero bridges/APs?**
Deliberate data design: min degree 3, 2-edge-connected mesh. Disasters
create critical structure — that before/after is the demo.

**Q10. Union-Find complexity?**
Path halving + union by rank → amortised inverse-Ackermann α(n) ≈
constant. `find` stays a 3-line iterative loop.

**Q11. Prim vs Kruskal — when which?**
Prim O(E log V) likes dense graphs (grow one tree); Kruskal O(E log E)
likes sparse edge lists. We run both; equal totals (28.068 km) because
distinct weights ⇒ unique MST.

**Q12. Why is greedy dispatch not globally optimal, and why use it?**
Locally best per incident; global optimum = Hungarian / min-cost
matching O(n³). Dispatch is an *online* problem — incidents arrive over
time — so real CAD systems use greedy-style policies.

**Q13. Why one Dijkstra per incident instead of per vehicle?**
`dijkstraAll` from the incident yields distances to every vehicle at
once (undirected symmetry). V× less work, same greedy result.

**Q14. Knapsack recurrence + how do you recover the item set?**
`dp[i][w] = max(dp[i-1][w], dp[i-1][w-wt]+val)`. Backtrack: item taken
iff `dp[i][w] != dp[i-1][w]`. Weights must be integers (kg).

**Q15. Bellman-Ford — why keep it when Dijkstra is faster?**
Handles negative weights (Dijkstra's settle-once breaks), one extra
pass detects negative cycles, and it's the ancestor of distance-vector
routing. Here it's the control experiment: same 8.11 km, different
work profile.

**Q16. Floyd-Warshall recurrence and cost here?**
`dist[i][j] = min(dist[i][j], dist[i][k]+dist[k][j])` over k. 28³ ≈
22k steps ≈ 30 µs → whole-city analytics (diameter 8.114 km, avg
3.993 km, Bus Terminal most central).

**Q17. Where is BFS *as a data structure* (queue)?**
Plain vector + head index (`max_flow.cpp`, `disaster.cpp`) — an
explicit array-backed queue, no `std::queue`.

**Q18. How is severity decided in auto-dispatch?**
By BFS wave: epicenter = 5, ring 1 = 3, ring 2 = 1 — closer to the
epicenter, more critical. Ties in the triage heap are FIFO via a
sequence number (stable).

**Q19. Thread safety?**
httplib is multithreaded; one mutex guards graph mutations + history.
Immutable data (Trie, full-graph MST inputs) is read lock-free — and we
can say why.

**Q20. How do you KNOW the whole thing is correct?**
96 automated checks across 5 suites; hand-verified paper graphs;
cross-algorithm agreement (Dijkstra=A*=BF=FW, Prim=Kruskal, flow=cut);
and independent verification against Python networkx for bridges, APs,
MST total, diameter, and centrality.

---

## 3. Per-member cheat sheet (60 seconds each)

**M1 Routing** — graph.h is an adjacency list; Dijkstra settles nearest
first via our MinHeap (lazy deletion); A* adds haversine h (admissible
by data construction); `dijkstraAll` powers dispatch; BF/FW are the
comparison lab. Know Q1–Q4, Q13, Q15–Q16.

**M2 Evacuation** — BFS rings spread the disaster and block in-zone
roads; Edmonds-Karp on road capacities gives people/hour; residual BFS
gives the min cut = bottlenecks; flow == cut sum is the proof. Q5–Q7.

**M3 Dispatch** — MaxHeap triage (stable ties), greedy nearest free
compatible vehicle by road distance, knapsack DP for the truck (with
backtracking), HashMap fleet counts, History = SQLite or JSONL via
`#ifdef`. Q12–Q14, Q18.

**M4 Resilience** — one Tarjan DFS → bridges + APs (disc/low); healthy
mesh has none, disasters create them; Union-Find (path halving + rank)
for Kruskal & components; Prim vs Kruskal cross-check; heapsort inside
Kruskal. Q8–Q11.

**M5 Frontend** — React (Vite) command center; App.jsx owns state,
MapView bridges React↔Leaflet imperatively; api.js mirrors
docs/api_spec.md; Trie search box; vite proxy in dev, C++ serves
`dist/` in prod. Can demo everything end-to-end.

---

## 4. Two-minute master demo

1. **Search** `riv` → Trie suggestions → fly to Riverside Colony.
2. **Route** Riverside → Sunrise Hospital, **Compare**: same 4.78 km,
   Dijkstra 24 vs A* 8 nodes.
3. **Flood** River Bridge South (1 ring): roads dash red, route
   reroutes to 5.42 km over the north bridge.
4. **Evacuation**: people/hour + min-cut bottleneck roads.
5. **Dispatch**: severity badges (wave-based), amber vehicle paths,
   one unreachable incident — "yehi min-cut ka point hai."
6. **Analyze network**: 5 violet critical roads + 7 critical junctions
   appear (healthy city had zero).
7. **Restoration plan**: 28.07 km green backbone, "Prim = Kruskal."
8. **Algorithm lab**: 3 algorithms race → same distance, different
   effort; **City stats**: diameter, average, most central — 22k FW ops
   in ~30 µs.
9. Close: `./lifeline console` — the whole engine with zero UI.

---

## 5. Architecture (one breath)

```
React (Vite build)  ──HTTP/JSON──▶  cpp-httplib REST layer
                                         │ one mutex
                    ┌────────────────────┼─────────────────────┐
              routing/            evacuation/            dispatch/
        Dijkstra·A*·BF·FW      Edmonds-Karp+cut     triage·greedy·knapsack
                    └───────── core/graph.h ◀── data/city_graph.json
                                    ▲
                 resilience/ (Tarjan·UF·MST)   ds/ (heaps·hashmap·trie)
                                    │
                          db/history (SQLite | JSONL)
```
