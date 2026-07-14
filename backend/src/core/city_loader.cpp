#include "core/city_loader.h"

#include <fstream>
#include <stdexcept>

#include "json.hpp"

namespace lifeline {

using nlohmann::json;

Graph loadCityFromJson(const std::string& path, std::string* cityName) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("cannot open city file: " + path);

    json j;
    try {
        in >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("invalid JSON in " + path + ": " + e.what());
    }

    if (cityName) *cityName = j.value("city", "Unknown City");

    Graph g;

    // ---- nodes (must be listed in id order 0..N-1)
    for (const auto& n : j.at("nodes")) {
        const int id = g.addNode(n.at("name").get<std::string>(),
                                 n.value("type", "place"),
                                 n.at("lat").get<double>(),
                                 n.at("lon").get<double>());
        if (id != n.at("id").get<int>())
            throw std::runtime_error(
                "node ids must be sequential 0..N-1 in " + path);
    }

    // ---- edges
    for (const auto& e : j.at("edges")) {
        const int u = e.at("from").get<int>();
        const int v = e.at("to").get<int>();
        const double len = e.at("length_km").get<double>();
        if (!g.validId(u) || !g.validId(v) || u == v)
            throw std::runtime_error("bad edge endpoints in " + path);
        if (len <= 0.0)
            throw std::runtime_error("edge length must be positive in " + path);
        g.addEdge(u, v, len, e.value("capacity", 0.0),
                  e.value("road", ""));
    }

    return g;
}

}  // namespace lifeline
