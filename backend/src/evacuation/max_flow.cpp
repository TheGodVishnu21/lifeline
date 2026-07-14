#include "evacuation/max_flow.h"

#include <algorithm>
#include <chrono>
#include <set>

namespace lifeline {

namespace {

constexpr double kInf = 1e15;
constexpr double kEps = 1e-9;

// One directed arc of the flow network. Arcs are created in pairs;
// `rev` is the index (inside adj[to]) of the partner arc, so pushing
// flow forward always adds residual capacity backward in O(1).
struct Arc {
    int to;
    double cap;     // REMAINING capacity (mutated during the algorithm)
    double orig;    // original capacity (needed to report the min cut)
    int rev;        // index of reverse arc in adj[to]
    bool isRoad;    // real road (true) vs super-source/sink arc (false)
    std::string road;
};

class FlowNetwork {
public:
    explicit FlowNetwork(int n) : adj_(n) {}

    // capF = capacity u->v, capB = capacity v->u.
    // Undirected road: capF = capB = c. Super arc: capB = 0.
    void addPair(int u, int v, double capF, double capB, bool isRoad,
                 const std::string& road) {
        adj_[u].push_back({v, capF, capF, (int)adj_[v].size(), isRoad, road});
        adj_[v].push_back({u, capB, capB, (int)adj_[u].size() - 1, isRoad, road});
    }

    // BFS for the shortest augmenting path (this BFS choice is exactly
    // what upgrades plain Ford-Fulkerson into Edmonds-Karp).
    bool bfsAugment(int S, int T, std::vector<int>& prevNode,
                    std::vector<int>& prevArc) {
        const int n = (int)adj_.size();
        prevNode.assign(n, -1);
        prevArc.assign(n, -1);
        std::vector<int> queue = {S};   // plain vector + head index
        prevNode[S] = S;
        for (size_t head = 0; head < queue.size(); ++head) {
            const int u = queue[head];
            for (int i = 0; i < (int)adj_[u].size(); ++i) {
                const Arc& a = adj_[u][i];
                if (prevNode[a.to] == -1 && a.cap > kEps) {
                    prevNode[a.to] = u;
                    prevArc[a.to] = i;
                    if (a.to == T) return true;
                    queue.push_back(a.to);
                }
            }
        }
        return false;
    }

    double augment(int S, int T, const std::vector<int>& prevNode,
                   const std::vector<int>& prevArc) {
        double f = kInf;
        for (int v = T; v != S; v = prevNode[v])
            f = std::min(f, adj_[prevNode[v]][prevArc[v]].cap);
        for (int v = T; v != S; v = prevNode[v]) {
            Arc& a = adj_[prevNode[v]][prevArc[v]];
            a.cap -= f;
            adj_[v][a.rev].cap += f;
        }
        return f;
    }

    // Residual reachability from S (for the min cut).
    std::vector<bool> reachable(int S) {
        std::vector<bool> vis(adj_.size(), false);
        std::vector<int> queue = {S};
        vis[S] = true;
        for (size_t head = 0; head < queue.size(); ++head)
            for (const Arc& a : adj_[queue[head]])
                if (!vis[a.to] && a.cap > kEps) {
                    vis[a.to] = true;
                    queue.push_back(a.to);
                }
        return vis;
    }

    const std::vector<std::vector<Arc>>& adj() const { return adj_; }

private:
    std::vector<std::vector<Arc>> adj_;
};

}  // namespace

EvacResult computeEvacuation(const Graph& g, std::vector<int> sources,
                             std::vector<int> sinks) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    EvacResult res;

    // ---- sanitise inputs: valid, unique, disjoint (source wins)
    std::set<int> src(sources.begin(), sources.end());
    std::set<int> snk;
    for (int t : sinks)
        if (g.validId(t) && !src.count(t)) snk.insert(t);
    for (auto it = src.begin(); it != src.end();)
        it = g.validId(*it) ? std::next(it) : src.erase(it);

    res.sources.assign(src.begin(), src.end());
    res.sinks.assign(snk.begin(), snk.end());
    if (src.empty() || snk.empty()) {
        res.runtime_us = std::chrono::duration<double, std::micro>(
                             Clock::now() - t0).count();
        return res;
    }

    // ---- build flow network: city nodes + S + T
    const int n = g.size(), S = n, T = n + 1;
    FlowNetwork net(n + 2);
    for (int u = 0; u < n; ++u)
        for (const Edge& e : g.neighbors(u))
            if (u < e.to && !e.blocked && e.capacity > kEps)
                net.addPair(u, e.to, e.capacity, e.capacity, true, e.road);
    for (int s : src) net.addPair(S, s, kInf, 0.0, false, "");
    for (int t : snk) net.addPair(t, T, kInf, 0.0, false, "");

    // ---- Edmonds-Karp main loop
    std::vector<int> prevNode, prevArc;
    while (net.bfsAugment(S, T, prevNode, prevArc)) {
        res.max_flow += net.augment(S, T, prevNode, prevArc);
        ++res.augmenting_paths;
    }

    // ---- min cut: reachable side of the residual graph
    const std::vector<bool> vis = net.reachable(S);
    for (int u = 0; u <= T; ++u) {
        if (!vis[u]) continue;
        for (const Arc& a : net.adj()[u])
            if (a.isRoad && !vis[a.to] && a.orig > kEps)
                res.bottlenecks.push_back({u, a.to, a.orig, a.road});
    }

    res.runtime_us = std::chrono::duration<double, std::micro>(
                         Clock::now() - t0).count();
    return res;
}

}  // namespace lifeline
