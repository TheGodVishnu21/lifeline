#include "routing/floyd_warshall.h"

#include <chrono>
#include <cmath>
#include <limits>

namespace lifeline {

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();
}

std::vector<int> AllPairs::path(int i, int j) const {
    std::vector<int> p;
    if (i < 0 || j < 0 || i >= n || j >= n || next[i][j] == -1) return p;
    p.push_back(i);
    while (i != j) {
        i = next[i][j];
        p.push_back(i);
    }
    return p;
}

AllPairs floydWarshall(const Graph& g) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();

    AllPairs ap;
    const int n = ap.n = g.size();
    ap.dist.assign(n, std::vector<double>(n, kInf));
    ap.next.assign(n, std::vector<int>(n, -1));

    // base cases: self = 0, direct roads
    for (int i = 0; i < n; ++i) {
        ap.dist[i][i] = 0.0;
        ap.next[i][i] = i;
    }
    for (int u = 0; u < n; ++u)
        for (const Edge& e : g.neighbors(u)) {
            if (e.blocked) continue;
            if (e.length_km < ap.dist[u][e.to]) {
                ap.dist[u][e.to] = e.length_km;
                ap.next[u][e.to] = e.to;
            }
        }

    // the DP: allow intermediates one at a time
    for (int k = 0; k < n; ++k)
        for (int i = 0; i < n; ++i) {
            if (ap.dist[i][k] == kInf) continue;   // skip useless rows
            for (int j = 0; j < n; ++j) {
                const double via = ap.dist[i][k] + ap.dist[k][j];
                if (via < ap.dist[i][j]) {
                    ap.dist[i][j] = via;
                    ap.next[i][j] = ap.next[i][k];
                }
            }
        }

    ap.runtime_us = std::chrono::duration<double, std::micro>(
                        Clock::now() - t0).count();
    return ap;
}

CityStats computeCityStats(const AllPairs& ap) {
    CityStats s;
    const int n = ap.n;
    double sum = 0.0;
    long long finite = 0;

    for (int i = 0; i < n; ++i) {
        double ecc = 0.0;
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            const double d = ap.dist[i][j];
            if (std::isinf(d)) {
                if (i < j) ++s.disconnected_pairs;
                continue;
            }
            sum += d;
            ++finite;
            if (d > ecc) ecc = d;
            if (d > s.diameter_km) {
                s.diameter_km = d;
                s.diameter_from = i;
                s.diameter_to = j;
            }
        }
        if (s.most_central == -1 || ecc < s.central_ecc_km) {
            s.most_central = i;
            s.central_ecc_km = ecc;
        }
        if (s.least_central == -1 || ecc > s.remote_ecc_km) {
            s.least_central = i;
            s.remote_ecc_km = ecc;
        }
    }
    if (finite > 0) s.avg_distance_km = sum / finite;
    return s;
}

}  // namespace lifeline
