#include "resilience/critical.h"

#include <algorithm>

namespace lifeline {

namespace {

struct Dfs {
    const Graph& g;
    std::vector<int>  disc, low, comp;
    std::vector<bool> isAp;
    std::vector<std::pair<int, int>> bridges;
    int timer = 0;

    explicit Dfs(const Graph& gr)
        : g(gr), disc(gr.size(), -1), low(gr.size(), 0),
          comp(gr.size(), -1), isAp(gr.size(), false) {}

    void run(int u, int parent, int compId) {
        disc[u] = low[u] = timer++;
        comp[u] = compId;
        int children = 0;

        for (const Edge& e : g.neighbors(u)) {
            if (e.blocked) continue;
            const int v = e.to;
            if (v == parent) { parent = -2; continue; }  // skip the tree
                                    // edge back to parent exactly ONCE
            if (disc[v] == -1) {                 // tree edge
                ++children;
                run(v, u, compId);
                low[u] = std::min(low[u], low[v]);

                if (low[v] > disc[u])            // bridge condition
                    bridges.push_back({std::min(u, v), std::max(u, v)});
                if (parent != -1 && low[v] >= disc[u])   // non-root AP
                    isAp[u] = true;
            } else {                             // back edge
                low[u] = std::min(low[u], disc[v]);
            }
        }
        if (parent == -1 && children >= 2)       // root AP rule
            isAp[u] = true;
    }
};

}  // namespace

CriticalReport analyzeCritical(const Graph& g) {
    CriticalReport rep;
    Dfs dfs(g);

    for (int u = 0; u < g.size(); ++u)
        if (dfs.disc[u] == -1) {
            dfs.run(u, -1, rep.components);
            ++rep.components;
        }

    rep.bridges = std::move(dfs.bridges);
    std::sort(rep.bridges.begin(), rep.bridges.end());

    for (int u = 0; u < g.size(); ++u)
        if (dfs.isAp[u]) rep.articulation_points.push_back(u);

    rep.component_members.assign(rep.components, {});
    for (int u = 0; u < g.size(); ++u)
        rep.component_members[dfs.comp[u]].push_back(u);
    std::sort(rep.component_members.begin(), rep.component_members.end(),
              [](const auto& a, const auto& b) { return a.size() > b.size(); });

    return rep;
}

}  // namespace lifeline
