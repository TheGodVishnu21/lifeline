// core/graph.h
// -----------------------------------------------------------------------
// LifeLine core city graph.  *** FROZEN INTERFACE ***
// Every module (routing / evacuation / dispatch / resilience) builds on
// this class. Do not change public signatures without telling the team.
//
// Representation: adjacency list (vector of Edge vectors).
//   - Undirected roads: addEdge() inserts BOTH directions.
//   - Weights = road length in km (always >= straight-line distance,
//     so the haversine heuristic used by A* stays admissible).
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace lifeline {

struct Node {
    int         id = -1;
    std::string name;
    std::string type;      // hospital / shelter / fire_station / ...
    double      lat = 0.0;
    double      lon = 0.0;
};

struct Edge {
    int         to = -1;
    double      length_km = 0.0;   // Dijkstra / A* weight
    double      capacity  = 0.0;   // people per hour (Phase 2: max flow)
    std::string road;              // display name
    bool        blocked = false;   // Phase 2+: disasters block roads
};

// Straight-line (great-circle) distance between two coordinates.
double haversineKm(double lat1, double lon1, double lat2, double lon2);

class Graph {
public:
    // Returns the new node's id (ids are assigned 0,1,2,... in order).
    int  addNode(const std::string& name, const std::string& type,
                 double lat, double lon);

    // Undirected road: stores u->v and v->u.
    void addEdge(int u, int v, double length_km, double capacity,
                 const std::string& road);

    int  size() const { return static_cast<int>(nodes_.size()); }
    bool validId(int id) const { return id >= 0 && id < size(); }

    const Node&              node(int id)      const { return nodes_[id]; }
    const std::vector<Node>& nodes()           const { return nodes_; }
    const std::vector<Edge>& neighbors(int id) const { return adj_[id]; }

    // Total undirected edge count.
    int edgeCount() const;

    // Case-insensitive exact name lookup; -1 if not found.
    // (Phase 4 upgrades autocomplete to a Trie.)
    int findByName(const std::string& name) const;

    // A* heuristic: straight-line km between two nodes.
    double heuristicKm(int u, int v) const;

    // --- Phase 2: disasters mutate road state -----------------------
    // Blocks/unblocks BOTH stored directions of road u-v.
    void setEdgeBlocked(int u, int v, bool blocked);
    // Reopens every road (disaster reset).
    void resetBlockedAll();
    // Currently blocked roads, each once as (u, v) with u < v.
    std::vector<std::pair<int, int>> blockedEdges() const;

private:
    std::vector<Node>              nodes_;
    std::vector<std::vector<Edge>> adj_;
};

}  // namespace lifeline
