// resilience/mst.h
// -----------------------------------------------------------------------
// Minimum Spanning Tree two ways — the RESTORATION PLAN:
// "what is the minimum total km of road/power line we must keep alive
//  so every zone stays connected?"
//
//   PRIM     — grow one tree from a seed, always adding the cheapest
//              edge leaving the tree. Uses OUR MinHeap (lazy deletion).
//              O(E log V). Best on dense graphs.
//   KRUSKAL  — take edges globally cheapest-first, skip any that form a
//              cycle. Cycle test = OUR UnionFind. Edge ordering is done
//              by pushing all edges through OUR MinHeap and popping them
//              in order — i.e. a HEAPSORT with our own heap, no
//              std::sort. O(E log E). Best on sparse graphs.
//
// Both must return the SAME total weight (the MST optimum; unique here
// because all edge lengths are distinct) — the tests assert Prim ==
// Kruskal == networkx ground truth. Different algorithms, one answer:
// that agreement is the correctness argument.
//
// PLANNING VIEW: runs on the FULL network, ignoring blocked flags —
// you plan the backbone for the rebuilt city, not the flooded one.
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

#include "core/graph.h"

namespace lifeline {

struct MstEdge {
    int         u, v;
    double      length_km;
    std::string road;
};

struct MstResult {
    std::string          algo;        // "prim" | "kruskal"
    std::vector<MstEdge> edges;       // n-1 edges on a connected graph
    double               total_km = 0.0;
    double               runtime_us = 0.0;
};

MstResult primMst(const Graph& g);
MstResult kruskalMst(const Graph& g);

}  // namespace lifeline
