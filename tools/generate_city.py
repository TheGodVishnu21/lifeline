#!/usr/bin/env python3
"""
Indrapur city graph generator for LifeLine.

Why a generator instead of hand-typing lengths?
  Edge length = haversine(straight line) * road_factor, with factor >= 1.05.
  This GUARANTEES every road is at least as long as the straight line,
  which keeps the A* haversine heuristic admissible (never overestimates).
  Hand-typed lengths could accidentally break A* correctness.

Usage:  python3 tools/generate_city.py            (from repo root)
Output: backend/data/city_graph.json
"""

import json
import math
import os
import sys
from collections import deque

R_EARTH_KM = 6371.0088


def haversine_km(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * R_EARTH_KM * math.asin(math.sqrt(a))


# ---------------------------------------------------------------- nodes
# (id order matters: must be 0..N-1, loader validates this)
# A river runs roughly north-south near lon 72.877.
# Nodes 26 & 27 are the ONLY crossings -> perfect bridge/articulation
# demo for the Phase-4 resilience module.
NODES = [
    # west bank
    (0,  "City Hospital",          "hospital",       19.088, 72.858),
    (1,  "Sunrise Hospital",       "hospital",       19.089, 72.894),
    (2,  "Green Valley Hospital",  "hospital",       19.058, 72.892),
    (3,  "Central Fire Station",   "fire_station",   19.079, 72.868),
    (4,  "North Fire Station",     "fire_station",   19.096, 72.890),
    (5,  "Police HQ",              "police",         19.073, 72.864),
    (6,  "East Police Post",       "police",         19.072, 72.897),
    (7,  "Indrapur Railway Stn",   "transit",        19.076, 72.856),
    (8,  "Bus Terminal",           "transit",        19.070, 72.884),
    (9,  "Community Shelter A",    "shelter",        19.093, 72.866),
    (10, "Community Shelter B",    "shelter",        19.083, 72.899),
    (11, "Stadium Shelter",        "shelter",        19.064, 72.884),
    (12, "Old Market",             "market",         19.081, 72.862),
    (13, "Tech Park",              "office",         19.090, 72.905),
    (14, "Indrapur University",    "education",      19.079, 72.906),
    (15, "Riverside Colony",       "residential",    19.068, 72.871),
    (16, "Hill View Colony",       "residential",    19.097, 72.900),
    (17, "Lake Town",              "residential",    19.063, 72.903),
    (18, "Green Park",             "residential",    19.087, 72.871),
    (19, "Industrial Area",        "industrial",     19.052, 72.898),
    (20, "Power Plant",            "infrastructure", 19.048, 72.888),
    (21, "Water Works",            "infrastructure", 19.055, 72.879),
    (22, "City Mall",              "market",         19.062, 72.860),
    (23, "Airport Rd Junction",    "junction",       19.086, 72.884),
    (24, "Ring Road North",        "junction",       19.098, 72.872),
    (25, "Ring Road South",        "junction",       19.056, 72.868),
    (26, "River Bridge South",     "bridge",         19.066, 72.877),
    (27, "River Bridge North",     "bridge",         19.090, 72.877),
]

# ------------------------------------------------------------- edges
# (u, v, road_factor, road_name, capacity people/hour)
# capacity is unused in Phase 1; Phase 2 max-flow will consume it.
EDGES = [
    # west bank internal
    (7, 12, 1.15, "Station Road",        4000),
    (7, 22, 1.20, "Mall Road",           3500),
    (7, 5,  1.10, "MG Road",             4000),
    (12, 0, 1.10, "Hospital Road",       3000),
    (12, 3, 1.10, "Market Street",       3000),
    (12, 5, 1.15, "Old Town Road",       2500),
    (0, 9,  1.10, "Care Avenue",         2500),
    (0, 18, 1.20, "Park Road",           2500),
    (3, 18, 1.10, "Fire Lane",           2800),
    (3, 15, 1.20, "South Fire Road",     2600),
    (5, 15, 1.10, "River West Road",     3000),
    (5, 22, 1.15, "Mall Link Road",      2800),
    (9, 24, 1.10, "North Road",          3200),
    (9, 18, 1.10, "Green Way",           2200),
    (18, 24, 1.10, "Ring West North",    3600),
    (15, 25, 1.20, "Ring West South",    3000),
    (22, 25, 1.10, "Ring South-West",    3400),
    # bridge approaches + crossings (only 2 river crossings!)
    (15, 26, 1.05, "South Bridge Approach", 4200),
    (25, 26, 1.10, "Ring South Link",       3600),
    (18, 27, 1.05, "North Bridge Approach", 4200),
    (24, 27, 1.10, "Ring North Link",       3800),
    (27, 23, 1.05, "North Bridge Road",     4500),
    (26, 8,  1.05, "South Bridge Road",     4500),
    (26, 21, 1.10, "Waterfront Road",       3000),
    (26, 11, 1.10, "Stadium Link",          3200),
    # east bank internal
    (23, 8,  1.10, "Central East Road",  3800),
    (23, 1,  1.10, "Sunrise Avenue",     3200),
    (23, 4,  1.15, "North East Road",    3000),
    (1, 4,   1.10, "Hospital North Rd",  2600),
    (1, 10,  1.10, "Shelter Road East",  2600),
    (1, 16,  1.15, "Hill Road",          2400),
    (4, 16,  1.10, "Hill North Road",    2400),
    (10, 4,  1.10, "NE Shelter Road",    2400),
    (16, 13, 1.10, "Tech Hill Road",     2600),
    (13, 14, 1.05, "Campus Road",        3000),
    (13, 10, 1.10, "Tech Shelter Lane",  2400),
    (14, 17, 1.10, "Lake Road",          2600),
    (14, 6,  1.15, "University Road",    2800),
    (6, 8,   1.10, "Police East Road",   3200),
    (6, 17,  1.10, "East Colony Road",   2400),
    (17, 19, 1.10, "Industrial Road",    2800),
    (19, 2,  1.10, "Green Valley Road",  2600),
    (19, 20, 1.05, "Power Road",         3000),
    (2, 11,  1.10, "Valley Stadium Rd",  2800),
    (2, 20,  1.15, "Plant South Road",   2400),
    (20, 21, 1.10, "Utility Road",       2600),
    (21, 11, 1.10, "Waterworks Road",    2800),
    (11, 8,  1.10, "Stadium Avenue",     3400),
]


def main():
    n = len(NODES)
    # -- validate node ids are 0..n-1 in order
    for i, nd in enumerate(NODES):
        assert nd[0] == i, f"node id mismatch at index {i}"

    coords = {nd[0]: (nd[3], nd[4]) for nd in NODES}
    seen = set()
    edges_out = []
    for (u, v, f, road, cap) in EDGES:
        assert u != v, f"self loop {u}"
        assert f >= 1.05, f"factor < 1.05 on {u}-{v} breaks A* admissibility"
        key = (min(u, v), max(u, v))
        assert key not in seen, f"duplicate edge {key}"
        seen.add(key)
        straight = haversine_km(*coords[u], *coords[v])
        edges_out.append({
            "from": u, "to": v,
            "length_km": round(straight * f, 3),
            "road": road,
            "capacity": cap,
        })

    # -- connectivity check (BFS)
    adj = [[] for _ in range(n)]
    for e in edges_out:
        adj[e["from"]].append(e["to"])
        adj[e["to"]].append(e["from"])
    vis = [False] * n
    q = deque([0]); vis[0] = True; cnt = 1
    while q:
        u = q.popleft()
        for w in adj[u]:
            if not vis[w]:
                vis[w] = True; cnt += 1; q.append(w)
    assert cnt == n, f"graph disconnected: reached {cnt}/{n}"

    city = {
        "city": "Indrapur",
        "nodes": [
            {"id": i, "name": nm, "type": tp, "lat": la, "lon": lo}
            for (i, nm, tp, la, lo) in NODES
        ],
        "edges": edges_out,
    }

    out = os.path.join(os.path.dirname(__file__), "..", "backend", "data", "city_graph.json")
    out = os.path.abspath(out)
    with open(out, "w") as fp:
        json.dump(city, fp, indent=2)

    degs = [len(a) for a in adj]
    print(f"OK  wrote {out}")
    print(f"    nodes={n} edges={len(edges_out)} "
          f"min_deg={min(degs)} max_deg={max(degs)} connected=yes")


if __name__ == "__main__":
    sys.exit(main())
