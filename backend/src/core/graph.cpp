#include "core/graph.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace lifeline {

namespace {
constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kPi = 3.14159265358979323846;

double toRad(double deg) { return deg * kPi / 180.0; }

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}
}  // namespace

double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double p1 = toRad(lat1), p2 = toRad(lat2);
    const double dp = toRad(lat2 - lat1);
    const double dl = toRad(lon2 - lon1);
    const double a = std::sin(dp / 2) * std::sin(dp / 2) +
                     std::cos(p1) * std::cos(p2) *
                     std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * kEarthRadiusKm * std::asin(std::sqrt(a));
}

int Graph::addNode(const std::string& name, const std::string& type,
                   double lat, double lon) {
    Node n;
    n.id = size();
    n.name = name;
    n.type = type;
    n.lat = lat;
    n.lon = lon;
    nodes_.push_back(n);
    adj_.emplace_back();
    return n.id;
}

void Graph::addEdge(int u, int v, double length_km, double capacity,
                    const std::string& road) {
    Edge e1;  e1.to = v;  e1.length_km = length_km;
    e1.capacity = capacity;  e1.road = road;
    Edge e2 = e1;  e2.to = u;
    adj_[u].push_back(e1);
    adj_[v].push_back(e2);
}

int Graph::edgeCount() const {
    int total = 0;
    for (const auto& list : adj_) total += static_cast<int>(list.size());
    return total / 2;  // each undirected road stored twice
}

int Graph::findByName(const std::string& name) const {
    const std::string target = toLower(name);
    for (const Node& n : nodes_)
        if (toLower(n.name) == target) return n.id;
    return -1;
}

void Graph::setEdgeBlocked(int u, int v, bool blocked) {
    for (Edge& e : adj_[u])
        if (e.to == v) e.blocked = blocked;
    for (Edge& e : adj_[v])
        if (e.to == u) e.blocked = blocked;
}

void Graph::resetBlockedAll() {
    for (auto& list : adj_)
        for (Edge& e : list) e.blocked = false;
}

std::vector<std::pair<int, int>> Graph::blockedEdges() const {
    std::vector<std::pair<int, int>> out;
    for (int u = 0; u < size(); ++u)
        for (const Edge& e : adj_[u])
            if (e.blocked && u < e.to) out.push_back({u, e.to});
    return out;
}

double Graph::heuristicKm(int u, int v) const {
    const Node& a = nodes_[u];
    const Node& b = nodes_[v];
    return haversineKm(a.lat, a.lon, b.lat, b.lon);
}

}  // namespace lifeline
