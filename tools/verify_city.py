"""Independent ground truth for LifeLine's Indrapur graph (no networkx).

Mirrors the C++ semantics exactly:
  - disaster  = BFS to `severity` hops, block edges with BOTH ends inside
  - bridges/APs = Tarjan on the graph with blocked edges removed
  - MST       = Prim/Kruskal on unblocked edges
  - stats     = Floyd-Warshall aggregates from floyd_warshall.cpp
"""

import json
import math
from collections import deque
from pathlib import Path

GRAPH_PATH = Path("D:/Projects/lifeline/backend/data/city_graph.json")
INF = float("inf")

city = json.loads(GRAPH_PATH.read_text())
nodes = city["nodes"]
edges = [(e["from"], e["to"], e["length_km"], e["road"]) for e in city["edges"]]
n = len(nodes)


def adjacency(blocked: set) -> list:
    """Undirected adjacency list of unblocked edges: adj[u] = [(v, length_km)]."""
    adj = [[] for _ in range(n)]
    for (u, v, w, _road) in edges:
        if (min(u, v), max(u, v)) in blocked:
            continue
        adj[u].append((v, w))
        adj[v].append((u, w))
    return adj


def simulate_disaster(epicenter: int, severity: int):
    """BFS `severity` hops; block every edge with both endpoints affected."""
    adj = adjacency(set())
    dist = [-1] * n
    dist[epicenter] = 0
    q = deque([epicenter])
    waves = [[epicenter]]
    while q:
        u = q.popleft()
        if dist[u] == severity:
            continue
        for (v, _w) in adj[u]:
            if dist[v] == -1:
                dist[v] = dist[u] + 1
                while len(waves) <= dist[v]:
                    waves.append([])
                waves[dist[v]].append(v)
                q.append(v)
    affected = {i for i in range(n) if 0 <= dist[i] <= severity}
    blocked = {(min(u, v), max(u, v))
               for (u, v, _w, _r) in edges if u in affected and v in affected}
    return affected, blocked, waves


def tarjan(blocked: set):
    """Bridges (sorted, u<v), articulation points (sorted), components."""
    adj = adjacency(blocked)
    disc = [-1] * n
    low = [0] * n
    is_ap = [False] * n
    bridges = []
    comp = [-1] * n
    timer = [0]

    def dfs(root):
        # iterative DFS: stack holds (node, parent, neighbour index)
        stack = [(root, -1, 0)]
        disc[root] = low[root] = timer[0]; timer[0] += 1
        comp[root] = root
        root_children = 0
        while stack:
            u, parent, idx = stack[-1]
            if idx < len(adj[u]):
                stack[-1] = (u, parent, idx + 1)
                v = adj[u][idx][0]
                if v == parent:
                    continue
                if disc[v] == -1:
                    disc[v] = low[v] = timer[0]; timer[0] += 1
                    comp[v] = root
                    if u == root:
                        root_children += 1
                    stack.append((v, u, 0))
                else:
                    low[u] = min(low[u], disc[v])
            else:
                stack.pop()
                if stack:
                    p = stack[-1][0]
                    low[p] = min(low[p], low[u])
                    if low[u] > disc[p]:
                        bridges.append((min(p, u), max(p, u)))
                    if p != root and low[u] >= disc[p]:
                        is_ap[p] = True
        if root_children > 1:
            is_ap[root] = True

    for i in range(n):
        if disc[i] == -1:
            dfs(i)

    groups = {}
    for i in range(n):
        groups.setdefault(comp[i], []).append(i)
    members = sorted(groups.values(), key=lambda g: (-len(g), g[0]))
    return sorted(bridges), [i for i in range(n) if is_ap[i]], members


def prim_mst(blocked: set):
    adj = adjacency(blocked)
    seen = [False] * n
    total, count = 0.0, 0
    import heapq
    pq = [(0.0, 0)]
    while pq:
        w, u = heapq.heappop(pq)
        if seen[u]:
            continue
        seen[u] = True
        total += w
        count += 1
        for (v, wt) in adj[u]:
            if not seen[v]:
                heapq.heappush(pq, (wt, v))
    return total, count - 1


def dijkstra(src: int, blocked: set):
    import heapq
    adj = adjacency(blocked)
    dist = [INF] * n
    prev = [-1] * n
    dist[src] = 0.0
    pq = [(0.0, src)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist[u] + 1e-12:
            continue
        for (v, w) in adj[u]:
            if d + w < dist[v] - 1e-12:
                dist[v] = d + w
                prev[v] = u
                heapq.heappush(pq, (dist[v], v))
    return dist, prev


def path_of(prev, src, dst):
    out, cur = [], dst
    while cur != -1:
        out.append(cur)
        if cur == src:
            break
        cur = prev[cur]
    return out[::-1]


def city_stats(blocked: set):
    """Same aggregation order as computeCityStats()."""
    allpairs = [dijkstra(s, blocked)[0] for s in range(n)]
    diameter, dia_pair = 0.0, (-1, -1)
    total, finite = 0.0, 0
    most_central, central_ecc = -1, 0.0
    disconnected = 0
    for i in range(n):
        ecc = 0.0
        for j in range(n):
            if i == j:
                continue
            d = allpairs[i][j]
            if math.isinf(d):
                if i < j:
                    disconnected += 1
                continue
            total += d
            finite += 1
            ecc = max(ecc, d)
            if d > diameter:
                diameter, dia_pair = d, (i, j)
        if most_central == -1 or ecc < central_ecc:
            most_central, central_ecc = i, ecc
    return {
        "diameter_km": diameter,
        "diameter_pair": dia_pair,
        "avg_distance_km": total / finite if finite else 0.0,
        "most_central": most_central,
        "central_ecc_km": central_ecc,
        "disconnected_pairs": disconnected,
    }


name_of = {nd["id"]: nd["name"] for nd in nodes}

print(f"nodes = {n}, edges = {len(edges)}")

healthy_bridges, healthy_aps, healthy_comps = tarjan(set())
print(f"healthy: bridges={healthy_bridges} aps={healthy_aps} "
      f"components={len(healthy_comps)}")

mst_total, mst_edges = prim_mst(set())
print(f"MST: {mst_edges} edges, total = {mst_total:.6f} km")

stats = city_stats(set())
print("healthy stats:")
for k, v in stats.items():
    print(f"    {k} = {v}")
print(f"    diameter pair names = {name_of[stats['diameter_pair'][0]]} <-> "
      f"{name_of[stats['diameter_pair'][1]]}")
print(f"    most central name   = {name_of[stats['most_central']]}")

affected, blocked, waves = simulate_disaster(26, 1)
print(f"\nflood@26 sev1: affected={len(affected)} blocked={len(blocked)} "
      f"waves={len(waves)} wave0={waves[0]}")
fb, fa, fc = tarjan(blocked)
print(f"    components={len(fc)} last_component={fc[-1]}")
print(f"    bridges={fb}")
print(f"    aps={fa}")

dist, prev = dijkstra(15, blocked)
p = path_of(prev, 15, 1)
print(f"    route 15->1: {dist[1]:.4f} km via {p} (uses 27: {27 in p})")

healthy_dist, _ = dijkstra(15, set())
print(f"    healthy 15->1 = {healthy_dist[1]:.4f} km")

riv = sorted(nd["id"] for nd in nodes if nd["name"].lower().startswith("riv"))
ring = sorted(nd["id"] for nd in nodes if nd["name"].lower().startswith("ring road"))
print(f"\ntrie: size={n} 'riv'->{riv} 'ring road'->{ring}")
